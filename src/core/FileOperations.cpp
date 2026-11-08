#include <gio/gio.h>

#include "core/FileOperations.h"

#include "core/Archive.h"
#include "core/PathUtils.h"
#include "core/TrashService.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMetaMethod>
#include <QProcess>

#include <dirent.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QSet>

#include <atomic>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <functional>

namespace {
constexpr qsizetype kCopyBuffer = 256 * 1024;

#ifndef RENAME_NOREPLACE
constexpr unsigned kRenameNoReplace = 1;
#else
constexpr unsigned kRenameNoReplace = RENAME_NOREPLACE;
#endif

bool pathExists(const QByteArray &path)
{
    struct stat st {};
    return ::lstat(path.constData(), &st) == 0;
}

bool isRootPath(const QByteArray &path)
{
    return path.isEmpty() || path == QByteArrayLiteral("/");
}

QByteArray baseName(const QByteArray &path)
{
    const qsizetype slash = path.lastIndexOf('/');
    return slash < 0 ? path : path.mid(slash + 1);
}

QByteArray parentPath(const QByteArray &path)
{
    const qsizetype slash = path.lastIndexOf('/');
    if (slash <= 0)
        return QByteArrayLiteral("/");
    return path.left(slash);
}

bool isSameOrDescendant(const QByteArray &path, const QByteArray &ancestor)
{
    if (path == ancestor)
        return true;
    if (ancestor == QByteArrayLiteral("/"))
        return path.startsWith('/');
    if (!path.startsWith(ancestor))
        return false;
    return path.size() > ancestor.size() && path.at(ancestor.size()) == '/';
}

QString errorFor(const QByteArray &path, int code)
{
    return QStringLiteral("%1: %2")
        .arg(PathUtils::displayName(path), QString::fromLocal8Bit(std::strerror(code)));
}

QByteArray uniqueName(const QByteArray &directory, const QByteArray &name, bool directoryEntry)
{
    QByteArray stem = name;
    QByteArray extension;

    if (!directoryEntry) {
        const qsizetype dot = name.lastIndexOf('.');
        if (dot > 0) {
            stem = name.left(dot);
            extension = name.mid(dot);
            if (stem.size() > 4 && stem.right(4).toLower() == QByteArrayLiteral(".tar")) {
                extension = stem.right(4) + extension;
                stem.chop(4);
            }
        }
    }

    for (int index = 1; index < 100000; ++index) {
        const QByteArray candidate = stem + " (" + QByteArray::number(index) + ")" + extension;
        if (!pathExists(PathUtils::childBytes(directory, candidate)))
            return candidate;
    }

    return name;
}

bool validName(const QString &name, QString *error)
{
    if (name.isEmpty()) {
        *error = QCoreApplication::translate("Ooze", "El nombre no puede estar vacío");
        return false;
    }
    if (name == QStringLiteral(".") || name == QStringLiteral("..")) {
        *error = QCoreApplication::translate("Ooze", "Ese nombre no es válido");
        return false;
    }
    if (name.contains('/') || name.contains(QChar(0))) {
        *error = QCoreApplication::translate("Ooze", "El nombre no puede contener «/»");
        return false;
    }
    return true;
}

QString countText(int count, const QString &singular, const QString &plural)
{
    return QStringLiteral("%1 %2").arg(count).arg(count == 1 ? singular : plural);
}

struct OperationContext
{
    std::function<void(const QString &)> progress;
    QString error;
};

bool copyFile(const QByteArray &source, const QByteArray &target, const struct stat &st,
              OperationContext &context)
{
    const int input = ::open(source.constData(), O_RDONLY | O_CLOEXEC);
    if (input < 0) {
        context.error = errorFor(source, errno);
        return false;
    }

    const int output = ::open(target.constData(),
                              O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                              mode_t(st.st_mode & 07777));
    if (output < 0) {
        context.error = errorFor(target, errno);
        ::close(input);
        return false;
    }

    bool ok = true;
    bool cloned = false;

    if (st.st_size > 0) {
        cloned = ::ioctl(output, FICLONE, input) == 0;
        if (!cloned) {
            (void)::ftruncate(output, 0);
            (void)::lseek(input, 0, SEEK_SET);
        }
    } else {
        cloned = true;
    }

    if (!cloned) {
        char buffer[kCopyBuffer];
        for (;;) {
            const ssize_t readBytes = ::read(input, buffer, sizeof(buffer));
            if (readBytes < 0) {
                if (errno == EINTR)
                    continue;
                context.error = errorFor(source, errno);
                ok = false;
                break;
            }
            if (readBytes == 0)
                break;

            qsizetype written = 0;
            while (written < readBytes) {
                const ssize_t chunk = ::write(output, buffer + written,
                                              size_t(readBytes - written));
                if (chunk < 0) {
                    if (errno == EINTR)
                        continue;
                    context.error = errorFor(target, errno);
                    ok = false;
                    break;
                }
                written += chunk;
            }

            if (!ok)
                break;
        }
    }

    const struct timespec times[2] = {st.st_atim, st.st_mtim};
    ::futimens(output, times);
    ::fchmod(output, mode_t(st.st_mode & 07777));

    ::close(output);
    ::close(input);

    if (!ok)
        ::unlink(target.constData());
    return ok;
}

bool copyEntry(const QByteArray &source, const QByteArray &target, OperationContext &context)
{
    struct stat st {};
    if (::lstat(source.constData(), &st) != 0) {
        context.error = errorFor(source, errno);
        return false;
    }

    if (context.progress)
        context.progress(PathUtils::displayName(baseName(source)));

    if (S_ISLNK(st.st_mode)) {
        QByteArray linked(4096, '\0');
        const ssize_t length = ::readlink(source.constData(), linked.data(), size_t(linked.size() - 1));
        if (length < 0) {
            context.error = errorFor(source, errno);
            return false;
        }
        linked.resize(length);
        if (::symlink(linked.constData(), target.constData()) != 0) {
            context.error = errorFor(target, errno);
            return false;
        }
        const struct timespec times[2] = {st.st_atim, st.st_mtim};
        ::utimensat(AT_FDCWD, target.constData(), times, AT_SYMLINK_NOFOLLOW);
        return true;
    }

    if (S_ISDIR(st.st_mode)) {
        const mode_t finalMode = mode_t(st.st_mode & 07777);
        if (::mkdir(target.constData(), finalMode | S_IRWXU) != 0) {
            context.error = errorFor(target, errno);
            return false;
        }

        DIR *dir = ::opendir(source.constData());
        if (!dir) {
            context.error = errorFor(source, errno);
            return false;
        }

        bool ok = true;
        while (struct dirent *entry = ::readdir(dir)) {
            const QByteArray name(entry->d_name);
            if (name == "." || name == "..")
                continue;
            if (!copyEntry(PathUtils::childBytes(source, name),
                           PathUtils::childBytes(target, name), context)) {
                ok = false;
                break;
            }
        }
        ::closedir(dir);

        if (!ok) {
            ::rmdir(target.constData());
            return false;
        }

        const struct timespec times[2] = {st.st_atim, st.st_mtim};
        ::chmod(target.constData(), finalMode);
        ::utimensat(AT_FDCWD, target.constData(), times, 0);
        return true;
    }

    if (!S_ISREG(st.st_mode)) {
        context.error = QCoreApplication::translate("Ooze", "%1: tipo de archivo no soportado")
                            .arg(PathUtils::displayName(source));
        return false;
    }

    return copyFile(source, target, st, context);
}

bool removeEntry(const QByteArray &path, OperationContext &context)
{
    if (isRootPath(path)) {
        context.error = QCoreApplication::translate("Ooze", "No se puede eliminar la raíz del sistema");
        return false;
    }

    struct stat st {};
    if (::lstat(path.constData(), &st) != 0) {
        if (errno == ENOENT)
            return true;
        context.error = errorFor(path, errno);
        return false;
    }

    if (context.progress)
        context.progress(PathUtils::displayName(baseName(path)));

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = ::opendir(path.constData());
        if (!dir) {
            context.error = errorFor(path, errno);
            return false;
        }

        bool ok = true;
        while (struct dirent *entry = ::readdir(dir)) {
            const QByteArray name(entry->d_name);
            if (name == "." || name == "..")
                continue;
            if (!removeEntry(PathUtils::childBytes(path, name), context)) {
                ok = false;
                break;
            }
        }
        ::closedir(dir);

        if (!ok)
            return false;

        if (::rmdir(path.constData()) != 0) {
            context.error = errorFor(path, errno);
            return false;
        }
        return true;
    }

    if (::unlink(path.constData()) != 0) {
        context.error = errorFor(path, errno);
        return false;
    }
    return true;
}

bool roundTrips(const QByteArray &path)
{
    return QString::fromUtf8(path).toUtf8() == path;
}

bool trashEntry(const QByteArray &path, OperationContext &context, QString *inTrash = nullptr)
{
    if (inTrash && roundTrips(path)) {
        QString trashed;
        if (QFile::moveToTrash(QString::fromUtf8(path), &trashed) && !trashed.isEmpty()) {
            *inTrash = trashed;
            return true;
        }
    }

    GFile *file = g_file_new_for_path(path.constData());
    GError *error = nullptr;
    const gboolean ok = g_file_trash(file, nullptr, &error);

    if (!ok) {
        if (error) {
            context.error = QStringLiteral("%1: %2")
                                .arg(PathUtils::displayName(path),
                                     QString::fromUtf8(error->message));
            g_error_free(error);
        } else {
            context.error = QCoreApplication::translate("Ooze", "%1: no se pudo mover a la papelera")
                                .arg(PathUtils::displayName(path));
        }
    }

    g_object_unref(file);
    return ok;
}

bool moveEntry(const QByteArray &source, const QByteArray &target, OperationContext &context)
{
    if (pathExists(target)) {
        context.error = QCoreApplication::translate("Ooze", "Ya existe un elemento en %1")
                            .arg(PathUtils::displayName(target));
        return false;
    }

    if (::rename(source.constData(), target.constData()) == 0)
        return true;

    if (errno != EXDEV) {
        context.error = errorFor(source, errno);
        return false;
    }

    if (!copyEntry(source, target, context)) {
        OperationContext cleanup;
        removeEntry(target, cleanup);
        return false;
    }

    return removeEntry(source, context);
}

bool restoreFromTrash(const QByteArray &trashed, const QByteArray &original,
                      OperationContext &context)
{
    if (trashed.isEmpty() || !pathExists(trashed)) {
        context.error = QCoreApplication::translate("Ooze", "El elemento ya no está en la papelera");
        return false;
    }

    if (pathExists(original)) {
        context.error = QCoreApplication::translate("Ooze", "Ya existe un elemento en %1")
                            .arg(PathUtils::displayName(original));
        return false;
    }

    struct stat parent {};
    const QByteArray parentDir = parentPath(original);
    if (::stat(parentDir.constData(), &parent) != 0 || !S_ISDIR(parent.st_mode)) {
        context.error = QCoreApplication::translate("Ooze", "La carpeta original ya no existe");
        return false;
    }

    if (!moveEntry(trashed, original, context))
        return false;

    TrashItem item;
    if (TrashService::locate(trashed, &item)) {
        TrashService::forget(item);
        return true;
    }

    const QByteArray filesDir = parentPath(trashed);
    if (baseName(filesDir) == "files") {
        const QByteArray info = PathUtils::childBytes(
            PathUtils::childBytes(parentPath(filesDir), "info"),
            baseName(trashed) + ".trashinfo");
        ::unlink(info.constData());
    }

    return true;
}

bool renameNoReplace(const QByteArray &from, const QByteArray &to, int *error)
{
    if (::renameat2(AT_FDCWD, from.constData(), AT_FDCWD, to.constData(), kRenameNoReplace) == 0)
        return true;

    if (errno != EINVAL && errno != ENOSYS && errno != ENOTSUP) {
        *error = errno;
        return false;
    }

    if (pathExists(to)) {
        *error = EEXIST;
        return false;
    }

    if (::rename(from.constData(), to.constData()) == 0)
        return true;
    *error = errno;
    return false;
}

QByteArray temporaryPath(const QByteArray &directory)
{
    static std::atomic<int> counter{0};
    for (;;) {
        const QByteArray candidate = PathUtils::childBytes(
            directory,
            QByteArrayLiteral(".ooze-tmp-") + QByteArray::number(::getpid()) + '-'
                + QByteArray::number(counter.fetch_add(1)));
        if (!pathExists(candidate))
            return candidate;
    }
}

void discardEntry(const QByteArray &path)
{
    OperationContext cleanup;
    removeEntry(path, cleanup);
}

class OperationTask : public QRunnable
{
public:
    OperationTask(FileOperations *owner, int action)
        : m_owner(owner)
        , m_action(action)
    {
    }

protected:
    void start(const QString &description)
    {
        emit m_owner->operationStarted(description);
    }

    void progress(const QString &description)
    {
        if (!m_timer.isValid())
            m_timer.start();
        if (m_timer.elapsed() < 120)
            return;
        m_timer.restart();
        emit m_owner->operationProgress(description);
    }

    void note(JournalEntry::Kind kind, const QString &label, const QByteArray &first,
              const QByteArray &second = {})
    {
        if (!roundTrips(first) || !roundTrips(second))
            return;

        m_journal.kind = kind;
        m_journal.label = label;
        m_journal.firsts.append(QString::fromUtf8(first));
        m_journal.seconds.append(QString::fromUtf8(second));
    }

    void finish(bool success, const QString &message, const QString &createdName = {},
                bool createdIsDirectory = false)
    {
        if (!m_journal.firsts.isEmpty()) {
            emit m_owner->changeRecorded(m_journal.kind, m_journal.label, m_journal.firsts,
                                         m_journal.seconds);
        }

        emit m_owner->operationFinished(m_action, success, message, createdName,
                                        createdIsDirectory);
        if (!success)
            emit m_owner->errorOccurred(message);
    }

    FileOperations *m_owner = nullptr;
    int m_action = FileOperations::Copy;
    QElapsedTimer m_timer;
    JournalEntry m_journal;
};

class TransferTask : public OperationTask
{
public:
    TransferTask(FileOperations *owner, QVector<QUrl> sources, QUrl destination, bool move,
                 int action = -1)
        : OperationTask(owner, action >= 0 ? action : (move ? FileOperations::Move : FileOperations::Copy))
        , m_sources(std::move(sources))
        , m_destination(std::move(destination))
        , m_move(move)
    {
        for (int &value : m_remembered)
            value = -1;
    }

    void run() override
    {
        start(m_move ? QCoreApplication::translate("Ooze", "Moviendo…") : QCoreApplication::translate("Ooze", "Copiando…"));

        if (!m_destination.isLocalFile()) {
            finish(false, QCoreApplication::translate("Ooze", "El destino no es una carpeta local"));
            return;
        }

        const QByteArray destination = PathUtils::toLocalBytes(
            PathUtils::normalized(m_destination));
        struct stat destinationStat {};
        if (::stat(destination.constData(), &destinationStat) != 0
            || !S_ISDIR(destinationStat.st_mode)) {
            finish(false, QCoreApplication::translate("Ooze", "La carpeta de destino no existe"));
            return;
        }

        m_context.progress = [this](const QString &name) {
            progress(QStringLiteral("%1 %2…")
                         .arg(m_move ? QCoreApplication::translate("Ooze", "Moviendo")
                                     : QCoreApplication::translate("Ooze", "Copiando"),
                              name));
        };

        int done = 0;
        int skipped = 0;

        for (const QUrl &url : m_sources) {
            if (!url.isLocalFile()) {
                finish(false, QCoreApplication::translate("Ooze", "Solo se admiten archivos locales"));
                return;
            }

            const QByteArray source = PathUtils::toLocalBytes(PathUtils::normalized(url));
            struct stat st {};
            if (::lstat(source.constData(), &st) != 0) {
                finish(false, errorFor(source, errno));
                return;
            }

            const QByteArray name = baseName(source);
            if (isRootPath(source) || name.isEmpty()) {
                finish(false, QCoreApplication::translate("Ooze", "No se puede operar sobre la raíz del sistema"));
                return;
            }

            if (m_move && parentPath(source) == destination) {
                ++skipped;
                continue;
            }

            if (S_ISDIR(st.st_mode) && isSameOrDescendant(destination, source)) {
                finish(false, QCoreApplication::translate("Ooze", "No se puede %1 una carpeta dentro de sí misma")
                                  .arg(m_move ? QCoreApplication::translate("Ooze", "mover") : QCoreApplication::translate("Ooze", "copiar")));
                return;
            }

            QByteArray target = PathUtils::childBytes(destination, name);
            if (!m_move && parentPath(source) == destination && pathExists(target)) {
                target = PathUtils::childBytes(
                    destination, uniqueName(destination, name, S_ISDIR(st.st_mode)));
            }

            bool plain = true;
            QByteArray placed;
            const Outcome outcome = place(source, target, &plain, &placed);
            if (outcome == Outcome::Cancelled) {
                finish(false, QCoreApplication::translate("Ooze", "Operación cancelada"));
                return;
            }
            if (outcome == Outcome::Failed) {
                finish(false, m_context.error);
                return;
            }

            if (plain) {
                if (m_move)
                    note(JournalEntry::Moved, QCoreApplication::translate("Ooze", "Mover"), source, placed);
                else
                    note(JournalEntry::Created, QCoreApplication::translate("Ooze", "Copiar"), placed);
            }

            ++done;
        }

        QString message = countText(done, m_move ? QCoreApplication::translate("Ooze", "elemento movido")
                                                 : QCoreApplication::translate("Ooze", "elemento copiado"),
                                    m_move ? QCoreApplication::translate("Ooze", "elementos movidos")
                                           : QCoreApplication::translate("Ooze", "elementos copiados"));
        if (skipped > 0) {
            message += QCoreApplication::translate("Ooze", " (%1 ya estaban en la carpeta)")
                           .arg(skipped);
        }

        finish(true, message);
    }

protected:
    enum class Outcome { Done, Cancelled, Failed };

    Outcome place(const QByteArray &source, const QByteArray &target, bool *plain, QByteArray *placed)
    {
        struct stat sourceStat {};
        if (::lstat(source.constData(), &sourceStat) != 0) {
            m_context.error = errorFor(source, errno);
            return Outcome::Failed;
        }

        struct stat targetStat {};
        if (::lstat(target.constData(), &targetStat) != 0) {
            if (errno != ENOENT) {
                m_context.error = errorFor(target, errno);
                return Outcome::Failed;
            }
            *placed = target;
            return transferNew(source, target) ? Outcome::Done : Outcome::Failed;
        }

        const bool sameEntry = sourceStat.st_dev == targetStat.st_dev
            && sourceStat.st_ino == targetStat.st_ino;
        if (sameEntry && m_move) {
            *plain = false;
            *placed = target;
            return Outcome::Done;
        }

        const int decision = sameEntry ? int(FileOperations::ConflictRename)
                                       : decide(source, target, sourceStat, targetStat);

        switch (decision) {
        case FileOperations::ConflictCancel:
            return Outcome::Cancelled;
        case FileOperations::ConflictOverwrite:
            *plain = false;
            *placed = target;
            return replaceEntry(source, target, sourceStat, targetStat) ? Outcome::Done : Outcome::Failed;
        case FileOperations::ConflictMerge:
            *plain = false;
            *placed = target;
            return mergeDirectories(source, target);
        case FileOperations::ConflictMergeReplace: {
            *plain = false;
            *placed = target;
            ++m_replaceDepth;
            const Outcome merged = mergeDirectories(source, target);
            --m_replaceDepth;
            return merged;
        }
        default: {
            const QByteArray directory = parentPath(target);
            const QByteArray renamed = PathUtils::childBytes(
                directory, uniqueName(directory, baseName(target), S_ISDIR(sourceStat.st_mode)));
            *placed = renamed;
            return transferNew(source, renamed) ? Outcome::Done : Outcome::Failed;
        }
        }
    }

    int decide(const QByteArray &source, const QByteArray &target, const struct stat &sourceStat,
               const struct stat &targetStat)
    {
        const bool sourceDirectory = S_ISDIR(sourceStat.st_mode);
        const bool targetDirectory = S_ISDIR(targetStat.st_mode);
        const bool sourceFile = S_ISREG(sourceStat.st_mode) || S_ISLNK(sourceStat.st_mode);
        const bool targetFile = S_ISREG(targetStat.st_mode) || S_ISLNK(targetStat.st_mode);

        int key = 2;
        bool canOverwrite = false;
        bool canMerge = false;
        if (sourceDirectory && targetDirectory) {
            key = 1;
            canOverwrite = true;
            canMerge = true;
        } else if (sourceFile && targetFile) {
            key = 0;
            canOverwrite = true;
        }

        if (m_replaceDepth > 0) {
            if (key == 0)
                return FileOperations::ConflictOverwrite;
            if (key == 1)
                return FileOperations::ConflictMerge;
        }

        if (m_remembered[key] >= 0)
            return m_remembered[key];

        if (!m_owner->hasConflictHandler())
            return FileOperations::ConflictRename;

        QVariantMap info;
        info.insert(QStringLiteral("name"), PathUtils::displayName(baseName(target)));
        info.insert(QStringLiteral("folder"), PathUtils::displayName(parentPath(target)));
        info.insert(QStringLiteral("sourceIsDirectory"), sourceDirectory);
        info.insert(QStringLiteral("targetIsDirectory"), targetDirectory);
        info.insert(QStringLiteral("sourceSize"), S_ISREG(sourceStat.st_mode) ? qint64(sourceStat.st_size) : qint64(-1));
        info.insert(QStringLiteral("targetSize"), S_ISREG(targetStat.st_mode) ? qint64(targetStat.st_size) : qint64(-1));
        info.insert(QStringLiteral("sourceModified"),
                    qint64(sourceStat.st_mtim.tv_sec) * 1000 + sourceStat.st_mtim.tv_nsec / 1000000);
        info.insert(QStringLiteral("targetModified"),
                    qint64(targetStat.st_mtim.tv_sec) * 1000 + targetStat.st_mtim.tv_nsec / 1000000);
        info.insert(QStringLiteral("canOverwrite"), canOverwrite);
        info.insert(QStringLiteral("canMerge"), canMerge);
        info.insert(QStringLiteral("move"), m_move);
        info.insert(QStringLiteral("multiple"), m_sources.size() > 1 || sourceDirectory);

        bool applyToAll = false;
        const int decision = m_owner->awaitConflictDecision(info, &applyToAll);
        if (applyToAll)
            m_remembered[key] = decision;
        return decision;
    }

    bool transferNew(const QByteArray &source, const QByteArray &target)
    {
        if (m_move) {
            int error = 0;
            if (renameNoReplace(source, target, &error))
                return true;
            if (error != EXDEV) {
                m_context.error = errorFor(error == EEXIST ? target : source, error);
                return false;
            }
        }

        const QByteArray temporary = temporaryPath(parentPath(target));
        if (!copyEntry(source, temporary, m_context)) {
            discardEntry(temporary);
            return false;
        }

        int error = 0;
        if (!renameNoReplace(temporary, target, &error)) {
            discardEntry(temporary);
            m_context.error = errorFor(target, error);
            return false;
        }

        if (m_move)
            return removeEntry(source, m_context);
        return true;
    }

    bool replaceEntry(const QByteArray &source, const QByteArray &target,
                      const struct stat &sourceStat, const struct stat &targetStat)
    {
        if (S_ISDIR(sourceStat.st_mode) != S_ISDIR(targetStat.st_mode)) {
            m_context.error = QCoreApplication::translate("Ooze", "No se puede reemplazar un archivo por una carpeta ni al revés");
            return false;
        }

        const QByteArray directory = parentPath(target);
        const QByteArray temporary = temporaryPath(directory);

        bool stagedByRename = false;
        if (m_move) {
            if (::rename(source.constData(), temporary.constData()) == 0) {
                stagedByRename = true;
            } else if (errno != EXDEV) {
                m_context.error = errorFor(source, errno);
                return false;
            }
        }

        if (!stagedByRename && !copyEntry(source, temporary, m_context)) {
            discardEntry(temporary);
            return false;
        }

        const auto rollback = [&] {
            if (stagedByRename)
                (void)::rename(temporary.constData(), source.constData());
            else
                discardEntry(temporary);
        };

        QString inTrash;
        OperationContext trashContext;
        const bool trashed = trashEntry(target, trashContext, &inTrash);
        int error = 0;

        if (trashed) {
            if (!renameNoReplace(temporary, target, &error)) {
                m_context.error = QStringLiteral("%1 (%2)")
                                      .arg(errorFor(target, error),
                                           QCoreApplication::translate("Ooze", "el original está en la papelera"));
                rollback();
                return false;
            }
        } else if (!S_ISDIR(targetStat.st_mode)) {
            if (::rename(temporary.constData(), target.constData()) != 0) {
                m_context.error = errorFor(target, errno);
                rollback();
                return false;
            }
        } else {
            const QByteArray aside = temporaryPath(directory);
            if (::rename(target.constData(), aside.constData()) != 0) {
                m_context.error = errorFor(target, errno);
                rollback();
                return false;
            }
            if (!renameNoReplace(temporary, target, &error)) {
                (void)::rename(aside.constData(), target.constData());
                m_context.error = errorFor(target, error);
                rollback();
                return false;
            }
            discardEntry(aside);
        }

        if (m_move && !stagedByRename)
            return removeEntry(source, m_context);
        return true;
    }

    Outcome mergeDirectories(const QByteArray &source, const QByteArray &target)
    {
        DIR *dir = ::opendir(source.constData());
        if (!dir) {
            m_context.error = errorFor(source, errno);
            return Outcome::Failed;
        }

        QList<QByteArray> names;
        while (struct dirent *entry = ::readdir(dir)) {
            const QByteArray name(entry->d_name);
            if (name != "." && name != "..")
                names.append(name);
        }
        ::closedir(dir);

        for (const QByteArray &name : std::as_const(names)) {
            if (m_context.progress)
                m_context.progress(PathUtils::displayName(name));

            bool plain = true;
            QByteArray placed;
            const Outcome outcome = place(PathUtils::childBytes(source, name),
                                          PathUtils::childBytes(target, name), &plain, &placed);
            if (outcome != Outcome::Done)
                return outcome;
        }

        if (m_move)
            (void)::rmdir(source.constData());
        return Outcome::Done;
    }

    QVector<QUrl> m_sources;
    QUrl m_destination;
    bool m_move = false;
    OperationContext m_context;
    int m_remembered[4];
    int m_replaceDepth = 0;
};

class RestoreTask : public TransferTask
{
public:
    RestoreTask(FileOperations *owner, QVector<QUrl> items)
        : TransferTask(owner, {}, QUrl(), true, FileOperations::Restore)
        , m_items(std::move(items))
    {
    }

    void run() override
    {
        start(QCoreApplication::translate("Ooze", "Restaurando…"));

        m_context.progress = [this](const QString &name) {
            progress(QStringLiteral("%1 %2…").arg(QCoreApplication::translate("Ooze", "Restaurando"), name));
        };

        int done = 0;
        for (const QUrl &url : m_items) {
            if (!url.isLocalFile()) {
                finish(false, QCoreApplication::translate("Ooze", "Solo se admiten archivos locales"));
                return;
            }

            const QByteArray path = PathUtils::toLocalBytes(PathUtils::normalized(url));
            TrashItem item;
            if (!TrashService::locate(path, &item)) {
                finish(false, QCoreApplication::translate("Ooze", "Solo se pueden restaurar elementos de primer nivel de la papelera"));
                return;
            }

            if (!item.hasInfo || item.originalPath.isEmpty()) {
                finish(false, QCoreApplication::translate("Ooze", "%1: se desconoce su ubicación original")
                                  .arg(PathUtils::displayName(item.displayBytes())));
                return;
            }

            QByteArray target = item.originalPath;
            const QByteArray folder = parentPath(target);

            struct stat folderStat {};
            if (::stat(folder.constData(), &folderStat) != 0 || !S_ISDIR(folderStat.st_mode)) {
                const int decision = askMissingFolder(folder, baseName(target));
                if (decision == FileOperations::ConflictRestoreHome) {
                    target = PathUtils::childBytes(QDir::homePath().toUtf8(), baseName(target));
                } else if (decision == FileOperations::ConflictRecreate) {
                    if (!makePath(folder)) {
                        finish(false, errorFor(folder, errno));
                        return;
                    }
                } else {
                    finish(false, QCoreApplication::translate("Ooze", "Operación cancelada"));
                    return;
                }
            }

            bool plain = true;
            QByteArray placed;
            const Outcome outcome = place(item.path, target, &plain, &placed);
            if (outcome == Outcome::Cancelled) {
                finish(false, QCoreApplication::translate("Ooze", "Operación cancelada"));
                return;
            }
            if (outcome == Outcome::Failed) {
                finish(false, m_context.error);
                return;
            }

            TrashService::forget(item);
            ++done;
        }

        finish(true, countText(done, QCoreApplication::translate("Ooze", "elemento restaurado"),
                               QCoreApplication::translate("Ooze", "elementos restaurados")));
    }

private:
    int askMissingFolder(const QByteArray &folder, const QByteArray &name)
    {
        if (m_missingRemembered >= 0)
            return m_missingRemembered;

        if (!m_owner->hasConflictHandler())
            return FileOperations::ConflictRecreate;

        QVariantMap info;
        info.insert(QStringLiteral("kind"), QStringLiteral("missingFolder"));
        info.insert(QStringLiteral("name"), PathUtils::displayName(name));
        info.insert(QStringLiteral("folder"), PathUtils::homeAbbreviated(folder));
        info.insert(QStringLiteral("multiple"), m_items.size() > 1);

        bool applyToAll = false;
        int decision = m_owner->awaitConflictDecision(info, &applyToAll);
        if (decision != FileOperations::ConflictRecreate && decision != FileOperations::ConflictRestoreHome)
            decision = FileOperations::ConflictCancel;
        if (applyToAll)
            m_missingRemembered = decision;
        return decision;
    }

    static bool makePath(const QByteArray &path)
    {
        if (path.isEmpty() || path == "/")
            return true;

        struct stat st {};
        if (::stat(path.constData(), &st) == 0)
            return S_ISDIR(st.st_mode);

        if (!makePath(parentPath(path)))
            return false;

        if (::mkdir(path.constData(), 0755) == 0)
            return true;
        return errno == EEXIST;
    }

    QVector<QUrl> m_items;
    int m_missingRemembered = -1;
};

class TrashDeleteTask : public OperationTask
{
public:
    TrashDeleteTask(FileOperations *owner, QVector<QUrl> items, bool all)
        : OperationTask(owner, all ? FileOperations::EmptyTrash : FileOperations::Remove)
        , m_items(std::move(items))
        , m_all(all)
    {
    }

    void run() override
    {
        start(m_all ? QCoreApplication::translate("Ooze", "Vaciando la papelera…")
                    : QCoreApplication::translate("Ooze", "Eliminando…"));

        QVector<TrashItem> items;
        const QVector<TrashDir> dirs = TrashService::discoverDirs();

        if (m_all) {
            for (const TrashDir &dir : dirs)
                items += TrashService::items(dir);
        } else {
            for (const QUrl &url : m_items) {
                if (!url.isLocalFile()) {
                    finish(false, QCoreApplication::translate("Ooze", "Solo se admiten archivos locales"));
                    return;
                }

                TrashItem item;
                if (!TrashService::locate(PathUtils::toLocalBytes(PathUtils::normalized(url)), &item)) {
                    finish(false, QCoreApplication::translate("Ooze", "Solo se pueden eliminar elementos de primer nivel de la papelera"));
                    return;
                }
                items.append(item);
            }
        }

        int done = 0;
        for (const TrashItem &item : std::as_const(items)) {
            progress(QStringLiteral("%1 %2…").arg(QCoreApplication::translate("Ooze", "Eliminando"),
                                                    PathUtils::displayName(item.displayBytes())));
            QString error;
            if (!TrashService::removeItem(item, &error)) {
                finish(false, error);
                return;
            }
            ++done;
        }

        if (m_all) {
            for (const TrashDir &dir : dirs)
                TrashService::purgeOrphans(dir);
            finish(true, QCoreApplication::translate("Ooze", "Papelera vaciada"));
            return;
        }

        finish(true, countText(done, QCoreApplication::translate("Ooze", "elemento eliminado"),
                               QCoreApplication::translate("Ooze", "elementos eliminados")));
    }

private:
    QVector<QUrl> m_items;
    bool m_all = false;
};

class LinkTask : public OperationTask
{
public:
    LinkTask(FileOperations *owner, QVector<QUrl> sources, QUrl destination)
        : OperationTask(owner, FileOperations::Link)
        , m_sources(std::move(sources))
        , m_destination(std::move(destination))
    {
    }

    void run() override
    {
        start(QCoreApplication::translate("Ooze", "Enlazando…"));

        if (!m_destination.isLocalFile()) {
            finish(false, QCoreApplication::translate("Ooze", "El destino no es una carpeta local"));
            return;
        }

        const QByteArray destination = PathUtils::toLocalBytes(
            PathUtils::normalized(m_destination));
        struct stat destinationStat {};
        if (::stat(destination.constData(), &destinationStat) != 0
            || !S_ISDIR(destinationStat.st_mode)) {
            finish(false, QCoreApplication::translate("Ooze", "La carpeta de destino no existe"));
            return;
        }

        int done = 0;
        for (const QUrl &url : m_sources) {
            if (!url.isLocalFile()) {
                finish(false, QCoreApplication::translate("Ooze", "Solo se admiten archivos locales"));
                return;
            }

            const QByteArray source = PathUtils::toLocalBytes(PathUtils::normalized(url));
            struct stat st {};
            if (::lstat(source.constData(), &st) != 0) {
                finish(false, errorFor(source, errno));
                return;
            }

            const QByteArray name = baseName(source);
            if (isRootPath(source) || name.isEmpty()) {
                finish(false, QCoreApplication::translate("Ooze", "No se puede operar sobre la raíz del sistema"));
                return;
            }

            QByteArray target = PathUtils::childBytes(destination, name);
            if (pathExists(target)) {
                target = PathUtils::childBytes(
                    destination, uniqueName(destination, name, S_ISDIR(st.st_mode)));
            }

            if (::symlink(source.constData(), target.constData()) != 0) {
                finish(false, errorFor(target, errno));
                return;
            }

            note(JournalEntry::Created, QCoreApplication::translate("Ooze", "Crear enlace"), target);
            ++done;
            progress(QCoreApplication::translate("Ooze", "Enlazando %1…").arg(PathUtils::displayName(target)));
        }

        finish(true, countText(done, QCoreApplication::translate("Ooze", "enlace creado"),
                               QCoreApplication::translate("Ooze", "enlaces creados")));
    }

private:
    QVector<QUrl> m_sources;
    QUrl m_destination;
};

class TrashTask : public OperationTask
{
public:
    TrashTask(FileOperations *owner, QVector<QUrl> sources, bool permanent)
        : OperationTask(owner, permanent ? FileOperations::Remove : FileOperations::Trash)
        , m_sources(std::move(sources))
        , m_permanent(permanent)
    {
    }

    void run() override
    {
        start(m_permanent ? QCoreApplication::translate("Ooze", "Eliminando…")
                          : QCoreApplication::translate("Ooze", "Enviando a la papelera…"));

        int done = 0;
        for (const QUrl &url : m_sources) {
            if (!url.isLocalFile()) {
                finish(false, QCoreApplication::translate("Ooze", "Solo se admiten archivos locales"));
                return;
            }

            const QByteArray path = PathUtils::toLocalBytes(PathUtils::normalized(url));
            if (isRootPath(path)) {
                finish(false, QCoreApplication::translate("Ooze", "No se puede operar sobre la raíz del sistema"));
                return;
            }
            OperationContext context;
            context.progress = [this](const QString &name) {
                progress(QStringLiteral("%1 %2…")
                             .arg(m_permanent ? QCoreApplication::translate("Ooze", "Eliminando")
                                              : QCoreApplication::translate("Ooze", "Enviando a la papelera"),
                                  name));
            };

            QString inTrash;
            const bool ok = m_permanent ? removeEntry(path, context)
                                        : trashEntry(path, context, &inTrash);
            if (!ok) {
                finish(false, context.error);
                return;
            }

            if (!m_permanent && !inTrash.isEmpty()) {
                note(JournalEntry::Trashed, QCoreApplication::translate("Ooze", "Mover a la papelera"), path,
                     inTrash.toUtf8());
            }
            ++done;
        }

        finish(true, countText(done,
                               m_permanent ? QCoreApplication::translate("Ooze", "elemento eliminado")
                                           : QCoreApplication::translate("Ooze", "elemento enviado a la papelera"),
                               m_permanent ? QCoreApplication::translate("Ooze", "elementos eliminados")
                                           : QCoreApplication::translate("Ooze", "elementos enviados a la papelera")));
    }

private:
    QVector<QUrl> m_sources;
    bool m_permanent = false;
};

class RenameTask : public OperationTask
{
public:
    RenameTask(FileOperations *owner, QUrl source, QString newName)
        : OperationTask(owner, FileOperations::Rename)
        , m_source(std::move(source))
        , m_newName(std::move(newName))
    {
    }

    void run() override
    {
        start(QCoreApplication::translate("Ooze", "Renombrando…"));

        QString nameError;
        if (!validName(m_newName, &nameError)) {
            finish(false, nameError);
            return;
        }

        if (!m_source.isLocalFile()) {
            finish(false, QCoreApplication::translate("Ooze", "Solo se admiten archivos locales"));
            return;
        }

        const QByteArray source = PathUtils::toLocalBytes(PathUtils::normalized(m_source));
        if (isRootPath(source)) {
            finish(false, QCoreApplication::translate("Ooze", "No se puede renombrar la raíz del sistema"));
            return;
        }
        const QByteArray target = PathUtils::childBytes(parentPath(source),
                                                        m_newName.toUtf8());

        if (source == target) {
            finish(true, QCoreApplication::translate("Ooze", "Sin cambios"), m_newName);
            return;
        }

        struct stat st {};
        if (::lstat(source.constData(), &st) != 0) {
            finish(false, errorFor(source, errno));
            return;
        }

        if (pathExists(target)) {
            finish(false, QCoreApplication::translate("Ooze", "Ya existe un elemento con ese nombre"));
            return;
        }

        if (::rename(source.constData(), target.constData()) != 0) {
            finish(false, errorFor(source, errno));
            return;
        }

        note(JournalEntry::Moved, QCoreApplication::translate("Ooze", "Renombrar"), source, target);
        finish(true, QStringLiteral("Renombrado a %1").arg(m_newName), m_newName,
               S_ISDIR(st.st_mode));
    }

private:
    QUrl m_source;
    QString m_newName;
};

class RenameBatchTask : public OperationTask
{
public:
    RenameBatchTask(FileOperations *owner, QVector<QUrl> sources, QStringList newNames)
        : OperationTask(owner, FileOperations::Rename)
        , m_sources(std::move(sources))
        , m_names(std::move(newNames))
    {
    }

    void run() override
    {
        start(QCoreApplication::translate("Ooze", "Renombrando…"));

        if (m_sources.isEmpty() || m_sources.size() != m_names.size()) {
            finish(false, QCoreApplication::translate("Ooze", "No hay elementos que renombrar"));
            return;
        }

        struct Item
        {
            QByteArray original;
            QByteArray source;
            QByteArray target;
        };

        QVector<Item> items;
        items.reserve(m_sources.size());
        QSet<QByteArray> sources;
        QSet<QByteArray> targets;

        for (int index = 0; index < m_sources.size(); ++index) {
            QString nameError;
            const QString &name = m_names.at(index);
            if (!validName(name, &nameError)) {
                finish(false, nameError);
                return;
            }

            const QUrl &url = m_sources.at(index);
            if (!url.isLocalFile()) {
                finish(false, QCoreApplication::translate("Ooze", "Solo se admiten archivos locales"));
                return;
            }

            const QByteArray source = PathUtils::toLocalBytes(
                PathUtils::normalized(url));
            if (isRootPath(source)) {
                finish(false, QCoreApplication::translate("Ooze", "No se puede renombrar la raíz del sistema"));
                return;
            }

            if (sources.contains(source)) {
                finish(false, QCoreApplication::translate("Ooze", "Hay elementos repetidos"));
                return;
            }

            const QByteArray target = PathUtils::childBytes(parentPath(source),
                                                            name.toUtf8());
            if (targets.contains(target)) {
                finish(false, QCoreApplication::translate("Ooze", "Hay nombres nuevos repetidos"));
                return;
            }

            struct stat st {};
            if (::lstat(source.constData(), &st) != 0) {
                finish(false, errorFor(source, errno));
                return;
            }

            sources.insert(source);
            targets.insert(target);
            items.append({source, source, target});
        }

        // Reject conflicts with files that are not part of the batch before
        // touching anything, so a failed preview cannot partially apply.
        for (const Item &item : std::as_const(items)) {
            if (item.source == item.target)
                continue;
            if (pathExists(item.target) && !sources.contains(item.target)) {
                finish(false, QCoreApplication::translate("Ooze", "Ya existe un elemento llamado «%1»")
                                   .arg(PathUtils::displayName(item.target)));
                return;
            }
        }

        QVector<QPair<QByteArray, QByteArray>> applied;
        const auto rollback = [&applied] {
            for (auto it = applied.crbegin(); it != applied.crend(); ++it)
                (void)::rename(it->second.constData(), it->first.constData());
            applied.clear();
        };

        // Cycles (A→B, B→A) and chains (A→B, B→C) need every source that is
        // also somebody's target out of the way first.  Files that are not
        // targets stay put, so a crash leaves at most the temporary names of
        // the items involved in the cycle.
        int tempIndex = 0;
        for (Item &item : items) {
            if (!targets.contains(item.source))
                continue;

            QByteArray temporary;
            do {
                temporary = PathUtils::childBytes(
                    parentPath(item.source),
                    QByteArrayLiteral(".ooze-rename-") + QByteArray::number(::getpid())
                        + '-' + QByteArray::number(tempIndex++));
            } while (pathExists(temporary));

            if (::rename(item.source.constData(), temporary.constData()) != 0) {
                const QString error = errorFor(item.source, errno);
                rollback();
                finish(false, error);
                return;
            }

            applied.append({item.source, temporary});
            item.source = temporary;
        }

        int done = 0;
        for (Item &item : items) {
            if (item.source == item.target)
                continue;

            if (pathExists(item.target)) {
                rollback();
                finish(false, QCoreApplication::translate("Ooze", "Ya existe un elemento llamado «%1»")
                                   .arg(PathUtils::displayName(item.target)));
                return;
            }

            if (::rename(item.source.constData(), item.target.constData()) != 0) {
                const QString error = errorFor(item.target, errno);
                rollback();
                finish(false, error);
                return;
            }

            applied.append({item.source, item.target});

            if (item.original == item.target)
                continue;

            note(JournalEntry::Moved, QCoreApplication::translate("Ooze", "Renombrar"), item.original,
                 item.target);
            ++done;
            progress(QCoreApplication::translate("Ooze", "Renombrando %1…").arg(PathUtils::displayName(item.target)));
        }

        finish(true, countText(done, QCoreApplication::translate("Ooze", "elemento renombrado"),
                               QCoreApplication::translate("Ooze", "elementos renombrados")));
    }

private:
    QVector<QUrl> m_sources;
    QStringList m_names;
};

class CreateTask : public OperationTask
{
public:
    CreateTask(FileOperations *owner, QUrl directory, QString name, bool directoryEntry)
        : OperationTask(owner, directoryEntry ? FileOperations::CreateDirectory
                                              : FileOperations::CreateFile)
        , m_directory(std::move(directory))
        , m_name(std::move(name))
        , m_directoryEntry(directoryEntry)
    {
    }

    void run() override
    {
        start(m_directoryEntry ? QCoreApplication::translate("Ooze", "Creando carpeta…")
                               : QCoreApplication::translate("Ooze", "Creando archivo…"));

        QString nameError;
        if (!validName(m_name, &nameError)) {
            finish(false, nameError);
            return;
        }

        if (!m_directory.isLocalFile()) {
            finish(false, QCoreApplication::translate("Ooze", "Solo se admiten carpetas locales"));
            return;
        }

        const QByteArray directory = PathUtils::toLocalBytes(
            PathUtils::normalized(m_directory));
        const QByteArray target = PathUtils::childBytes(directory, m_name.toUtf8());

        if (pathExists(target)) {
            finish(false, QCoreApplication::translate("Ooze", "Ya existe un elemento con ese nombre"));
            return;
        }

        if (m_directoryEntry) {
            if (::mkdir(target.constData(), 0755) != 0) {
                finish(false, errorFor(target, errno));
                return;
            }
        } else {
            const int fd = ::open(target.constData(),
                                  O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
            if (fd < 0) {
                finish(false, errorFor(target, errno));
                return;
            }
            ::close(fd);
        }

        note(JournalEntry::Created,
             m_directoryEntry ? QCoreApplication::translate("Ooze", "Crear carpeta") : QCoreApplication::translate("Ooze", "Crear archivo"),
             target);
        finish(true,
               m_directoryEntry ? QCoreApplication::translate("Ooze", "Carpeta «%1» creada").arg(m_name)
                                : QCoreApplication::translate("Ooze", "Archivo «%1» creado").arg(m_name),
               m_name, m_directoryEntry);
    }

private:
    QUrl m_directory;
    QString m_name;
    bool m_directoryEntry = true;
};

struct ArchiveSources
{
    QByteArray directory;
    QStringList names;
    QString error;
};

ArchiveSources inspectArchiveSources(const QVector<QUrl> &sources)
{
    ArchiveSources result;
    for (const QUrl &url : sources) {
        if (!url.isLocalFile()) {
            result.error = QCoreApplication::translate("Ooze", "Solo se admiten archivos locales");
            return result;
        }

        const QByteArray path = PathUtils::toLocalBytes(PathUtils::normalized(url));
        if (isRootPath(path) || !roundTrips(path)) {
            result.error = QCoreApplication::translate("Ooze", "No se puede comprimir este elemento");
            return result;
        }

        const QByteArray parent = parentPath(path);
        if (result.directory.isEmpty()) {
            result.directory = parent;
        } else if (result.directory != parent) {
            result.error = QCoreApplication::translate("Ooze", "Los elementos deben estar en la misma carpeta");
            return result;
        }

        result.names.append(QString::fromUtf8(baseName(path)));
    }
    return result;
}

QString defaultArchiveBase(const ArchiveSources &sources)
{
    if (sources.names.isEmpty())
        return {};

    QString base;
    if (sources.names.size() == 1) {
        const QByteArray only = PathUtils::childBytes(sources.directory, sources.names.first().toUtf8());
        struct stat st {};
        const bool isDir = ::lstat(only.constData(), &st) == 0 && S_ISDIR(st.st_mode);
        base = isDir ? sources.names.first() : QFileInfo(sources.names.first()).completeBaseName();
        if (base.isEmpty())
            base = sources.names.first();
    } else {
        base = QString::fromUtf8(baseName(sources.directory));
        if (base.isEmpty())
            base = QCoreApplication::translate("Ooze", "Archivo");
    }
    return base;
}

QString cleanArchiveBase(QString name, const QString &suffix, QString *error)
{
    name = name.trimmed();
    if (name.endsWith(suffix, Qt::CaseInsensitive))
        name.chop(suffix.size());
    name = name.trimmed();

    if (!validName(name, error))
        return {};

    if (name.toUtf8().size() + suffix.toUtf8().size() > 240) {
        *error = QCoreApplication::translate("Ooze", "El nombre es demasiado largo");
        return {};
    }
    return name;
}

QByteArray freeName(const QByteArray &directory, const QString &base, const QString &suffix)
{
    QByteArray candidate = (base + suffix).toUtf8();
    for (int n = 2; pathExists(PathUtils::childBytes(directory, candidate)); ++n)
        candidate = QStringLiteral("%1 (%2)%3").arg(base).arg(n).arg(suffix).toUtf8();
    return candidate;
}

class ArchiveTask : public OperationTask
{
public:
    ArchiveTask(FileOperations *owner, QVector<QUrl> sources, QString formatId, QString baseName)
        : OperationTask(owner, FileOperations::Compress)
        , m_sources(std::move(sources))
        , m_format(std::move(formatId))
        , m_name(std::move(baseName))
    {
    }

    ArchiveTask(FileOperations *owner, QUrl archive)
        : OperationTask(owner, FileOperations::Extract)
        , m_archive(std::move(archive))
        , m_extract(true)
    {
    }

    void run() override
    {
        if (m_extract)
            runExtract();
        else
            runCompress();
    }

private:
    static QString firstLine(const QByteArray &text)
    {
        const QString all = QString::fromLocal8Bit(text).trimmed();
        return all.section(QLatin1Char('\n'), 0, 0).trimmed();
    }

    bool execute(const Archive::Command &command, QString *error)
    {
        QProcess process;
        process.setProgram(command.program);
        process.setArguments(command.arguments);
        if (!command.workingDirectory.isEmpty())
            process.setWorkingDirectory(command.workingDirectory);

        process.start();
        if (!process.waitForStarted(5000)) {
            *error = QCoreApplication::translate("Ooze", "No se pudo ejecutar %1").arg(command.program);
            return false;
        }

        process.waitForFinished(-1);

        const bool tolerant = command.program.endsWith(QLatin1String("unzip"))
            || command.program.endsWith(QLatin1String("tar"));
        const int code = process.exitCode();
        const bool ok = process.exitStatus() == QProcess::NormalExit
            && (code == 0 || (tolerant && code == 1));

        if (!ok) {
            const QString detail = firstLine(process.readAllStandardError());
            *error = detail.isEmpty()
                ? QCoreApplication::translate("Ooze", "La herramienta terminó con el código %1").arg(code)
                : detail;
        }

        return ok;
    }

    void runCompress()
    {
        start(QCoreApplication::translate("Ooze", "Comprimiendo…"));

        if (m_sources.isEmpty() || Archive::suffixForFormat(m_format).isEmpty()) {
            finish(false, QCoreApplication::translate("Ooze", "Formato de archivo no admitido"));
            return;
        }

        const ArchiveSources inspected = inspectArchiveSources(m_sources);
        if (!inspected.error.isEmpty()) {
            finish(false, inspected.error);
            return;
        }

        const QByteArray &directory = inspected.directory;
        const QStringList &names = inspected.names;
        const QString suffix = Archive::suffixForFormat(m_format);

        QString base = defaultArchiveBase(inspected);
        if (!m_name.isEmpty()) {
            QString nameError;
            base = cleanArchiveBase(m_name, suffix, &nameError);
            if (base.isEmpty()) {
                finish(false, nameError);
                return;
            }
        }

        const QByteArray archiveName = freeName(directory, base, suffix);
        const QByteArray archivePath = PathUtils::childBytes(directory, archiveName);

        QString error;
        const Archive::Command command = Archive::compressCommand(
            m_format, QString::fromUtf8(archivePath), names, QString::fromUtf8(directory), &error);
        if (!command.isValid()) {
            finish(false, error);
            return;
        }

        if (!execute(command, &error)) {
            OperationContext cleanup;
            removeEntry(archivePath, cleanup);
            finish(false, QCoreApplication::translate("Ooze", "No se pudo comprimir: %1").arg(error));
            return;
        }

        note(JournalEntry::Created, QCoreApplication::translate("Ooze", "Comprimir"), archivePath);
        finish(true, QStringLiteral("Creado %1").arg(QString::fromUtf8(archiveName)),
               QString::fromUtf8(archiveName), false);
    }

    void runExtract()
    {
        start(QCoreApplication::translate("Ooze", "Extrayendo…"));

        if (!m_archive.isLocalFile()) {
            finish(false, QCoreApplication::translate("Ooze", "Solo se admiten archivos locales"));
            return;
        }

        const QByteArray archivePath = PathUtils::toLocalBytes(PathUtils::normalized(m_archive));
        const QString fileName = QString::fromUtf8(baseName(archivePath));
        if (!roundTrips(archivePath) || !Archive::isArchiveName(fileName)) {
            finish(false, QCoreApplication::translate("Ooze", "No es un archivo comprimido conocido"));
            return;
        }

        const QByteArray directory = parentPath(archivePath);
        const QByteArray destinationName = freeName(
            directory, Archive::baseNameWithoutSuffix(fileName), QString());
        const QByteArray destination = PathUtils::childBytes(directory, destinationName);

        if (::mkdir(destination.constData(), 0755) != 0) {
            finish(false, errorFor(destination, errno));
            return;
        }

        QString error;
        const Archive::Command command = Archive::extractCommand(
            QString::fromUtf8(archivePath), QString::fromUtf8(destination), &error);
        if (!command.isValid()) {
            OperationContext cleanup;
            removeEntry(destination, cleanup);
            finish(false, error);
            return;
        }

        if (!execute(command, &error)) {
            OperationContext cleanup;
            removeEntry(destination, cleanup);
            finish(false, QCoreApplication::translate("Ooze", "No se pudo extraer: %1").arg(error));
            return;
        }

        note(JournalEntry::Created, QCoreApplication::translate("Ooze", "Extraer"), destination);
        finish(true, QCoreApplication::translate("Ooze", "Extraído en %1").arg(QString::fromUtf8(destinationName)),
               QString::fromUtf8(destinationName), true);
    }

    QVector<QUrl> m_sources;
    QString m_format;
    QString m_name;
    QUrl m_archive;
    bool m_extract = false;
};

class JournalTask : public OperationTask
{
public:
    JournalTask(FileOperations *owner, JournalEntry entry, bool undo)
        : OperationTask(owner, undo ? FileOperations::Undo : FileOperations::Redo)
        , m_entry(std::move(entry))
        , m_undo(undo)
    {
    }

    void run() override
    {
        start(m_undo ? QCoreApplication::translate("Ooze", "Deshaciendo…") : QCoreApplication::translate("Ooze", "Rehaciendo…"));

        QStringList seconds = m_entry.seconds;
        while (seconds.size() < m_entry.firsts.size())
            seconds.append(QString());

        OperationContext context;
        bool ok = true;

        for (int i = 0; i < m_entry.firsts.size() && ok; ++i) {
            const QByteArray first = m_entry.firsts.at(i).toUtf8();
            const QByteArray second = seconds.at(i).toUtf8();

            switch (m_entry.kind) {
            case JournalEntry::Moved:
                ok = m_undo ? moveEntry(second, first, context)
                            : moveEntry(first, second, context);
                break;
            case JournalEntry::Trashed:
                if (m_undo) {
                    ok = restoreFromTrash(second, first, context);
                } else {
                    QString inTrash;
                    ok = trashEntry(first, context, &inTrash);
                    if (ok)
                        seconds[i] = inTrash;
                }
                break;
            case JournalEntry::Created:
                if (m_undo) {
                    QString inTrash;
                    ok = trashEntry(first, context, &inTrash);
                    if (ok)
                        seconds[i] = inTrash;
                } else {
                    ok = restoreFromTrash(second, first, context);
                }
                break;
            }
        }

        const QString message = ok
            ? QStringLiteral("%1: %2").arg(m_undo ? QCoreApplication::translate("Ooze", "Deshecho")
                                                  : QCoreApplication::translate("Ooze", "Rehecho"),
                                           m_entry.label)
            : context.error;

        emit m_owner->journalApplied(m_undo, ok, message, m_entry.firsts, seconds);
        finish(ok, message);
    }

private:
    JournalEntry m_entry;
    bool m_undo = true;
};
}

FileOperations::FileOperations(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(1);
    m_pool.setObjectName(QStringLiteral("ooze-files-operations"));
}

FileOperations::~FileOperations()
{
    {
        QMutexLocker locker(&m_conflictMutex);
        m_shutdown = true;
        m_conflictPending = 0;
    }
    m_conflictWait.wakeAll();
}

bool FileOperations::hasConflictHandler() const
{
    return isSignalConnected(QMetaMethod::fromSignal(&FileOperations::conflictRequested));
}

int FileOperations::awaitConflictDecision(const QVariantMap &info, bool *applyToAll)
{
    int id = 0;
    {
        QMutexLocker locker(&m_conflictMutex);
        if (m_shutdown) {
            *applyToAll = false;
            return ConflictCancel;
        }
        id = ++m_conflictSequence;
        m_conflictPending = id;
        m_conflictDecision = ConflictCancel;
        m_conflictApplyAll = false;
    }

    QVariantMap payload = info;
    payload.insert(QStringLiteral("id"), id);
    emit conflictRequested(payload);

    QMutexLocker locker(&m_conflictMutex);
    while (m_conflictPending == id && !m_shutdown)
        m_conflictWait.wait(&m_conflictMutex);

    if (m_shutdown) {
        *applyToAll = false;
        return ConflictCancel;
    }

    *applyToAll = m_conflictApplyAll;
    return m_conflictDecision;
}

void FileOperations::resolveConflict(int id, int decision, bool applyToAll)
{
    {
        QMutexLocker locker(&m_conflictMutex);
        if (id != m_conflictPending)
            return;
        m_conflictDecision = decision >= ConflictRename && decision <= ConflictMergeReplace
            ? decision : int(ConflictCancel);
        m_conflictApplyAll = applyToAll;
        m_conflictPending = 0;
    }
    m_conflictWait.wakeAll();
}

void FileOperations::copy(const QVector<QUrl> &sources, const QUrl &destination)
{
    if (sources.isEmpty())
        return;
    m_pool.start(new TransferTask(this, sources, destination, false));
}

void FileOperations::move(const QVector<QUrl> &sources, const QUrl &destination)
{
    if (sources.isEmpty())
        return;
    m_pool.start(new TransferTask(this, sources, destination, true));
}

void FileOperations::link(const QVector<QUrl> &sources, const QUrl &destination)
{
    if (sources.isEmpty())
        return;
    m_pool.start(new LinkTask(this, sources, destination));
}

void FileOperations::trash(const QVector<QUrl> &sources)
{
    if (sources.isEmpty())
        return;
    m_pool.start(new TrashTask(this, sources, false));
}

void FileOperations::remove(const QVector<QUrl> &sources)
{
    if (sources.isEmpty())
        return;
    m_pool.start(new TrashTask(this, sources, true));
}

void FileOperations::restoreTrashItems(const QVector<QUrl> &items)
{
    if (items.isEmpty())
        return;
    m_pool.start(new RestoreTask(this, items));
}

void FileOperations::deleteTrashItems(const QVector<QUrl> &items)
{
    if (items.isEmpty())
        return;
    m_pool.start(new TrashDeleteTask(this, items, false));
}

void FileOperations::emptyTrash()
{
    m_pool.start(new TrashDeleteTask(this, {}, true));
}

void FileOperations::rename(const QUrl &source, const QString &newName)
{
    m_pool.start(new RenameTask(this, source, newName));
}

void FileOperations::renameBatch(const QVector<QUrl> &sources,
                                  const QStringList &newNames)
{
    if (sources.isEmpty() || sources.size() != newNames.size())
        return;
    m_pool.start(new RenameBatchTask(this, sources, newNames));
}

void FileOperations::createDirectory(const QUrl &directory, const QString &name)
{
    m_pool.start(new CreateTask(this, directory, name, true));
}

void FileOperations::createFile(const QUrl &directory, const QString &name)
{
    m_pool.start(new CreateTask(this, directory, name, false));
}

void FileOperations::compress(const QVector<QUrl> &sources, const QString &formatId,
                              const QString &baseName)
{
    if (sources.isEmpty())
        return;
    m_pool.start(new ArchiveTask(this, sources, formatId, baseName));
}

QString FileOperations::suggestedArchiveBase(const QVector<QUrl> &sources)
{
    const ArchiveSources inspected = inspectArchiveSources(sources);
    if (!inspected.error.isEmpty())
        return {};
    return defaultArchiveBase(inspected);
}

QString FileOperations::archiveTargetName(const QVector<QUrl> &sources, const QString &baseName,
                                          const QString &formatId, QString *error)
{
    QString localError;
    QString *out = error ? error : &localError;
    out->clear();

    const QString suffix = Archive::suffixForFormat(formatId);
    if (suffix.isEmpty()) {
        *out = QCoreApplication::translate("Ooze", "Formato de archivo no admitido");
        return {};
    }

    const ArchiveSources inspected = inspectArchiveSources(sources);
    if (!inspected.error.isEmpty()) {
        *out = inspected.error;
        return {};
    }

    const QString base = cleanArchiveBase(baseName, suffix, out);
    if (base.isEmpty())
        return {};

    return QString::fromUtf8(freeName(inspected.directory, base, suffix));
}

void FileOperations::extract(const QUrl &archive)
{
    m_pool.start(new ArchiveTask(this, archive));
}

bool FileOperations::busy() const
{
    return m_pool.activeThreadCount() > 0;
}

void FileOperations::applyJournal(const JournalEntry &entry, bool undo)
{
    if (entry.firsts.isEmpty())
        return;
    m_pool.start(new JournalTask(this, entry, undo));
}

QString FileOperations::suggestedName(const QUrl &directory, const QString &base,
                                      bool directoryEntry)
{
    if (!directory.isLocalFile() || base.isEmpty())
        return base;

    const QByteArray dir = PathUtils::toLocalBytes(PathUtils::normalized(directory));
    const QByteArray name = base.toUtf8();

    if (!pathExists(PathUtils::childBytes(dir, name)))
        return base;

    return QString::fromUtf8(uniqueName(dir, name, directoryEntry));
}
