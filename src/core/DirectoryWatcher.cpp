#include "core/DirectoryWatcher.h"

#include "core/PathUtils.h"
#include "core/RecentFiles.h"
#include "core/TrashService.h"

#include <QFileInfo>
#include <QFile>
#include <QSocketNotifier>
#include <QTimer>

#include <sys/inotify.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcWatch, "ooze.files.watch")

namespace {
constexpr int kDebounceMs = 180;
constexpr quint32 kWatchMask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO
                             | IN_ATTRIB | IN_CLOSE_WRITE | IN_DELETE_SELF
                             | IN_MOVE_SELF | IN_ONLYDIR;
}

DirectoryWatcher::DirectoryWatcher(QObject *parent)
    : QObject(parent)
{
    m_inotifyFd = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_inotifyFd < 0) {
        qCWarning(lcWatch) << "inotify no está disponible:" << std::strerror(errno);
    } else {
        m_notifier = new QSocketNotifier(m_inotifyFd, QSocketNotifier::Read, this);
        m_notifier->setEnabled(false);
        connect(m_notifier, &QSocketNotifier::activated, this,
                [this] { onActivated(); });
    }

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &DirectoryWatcher::changed);
}

DirectoryWatcher::~DirectoryWatcher()
{
    clearWatch();
    if (m_inotifyFd >= 0)
        ::close(m_inotifyFd);
}

bool DirectoryWatcher::watch(const QUrl &directory)
{
    const bool recent = directory.scheme() == QLatin1String("recent");
    const bool trash = directory.scheme() == QLatin1String("trash");
    if (!directory.isLocalFile() && !recent && !trash) {
        clearWatch();
        return false;
    }
    if (m_inotifyFd < 0)
        return false;

    const QUrl target = (recent || trash) ? directory : PathUtils::normalized(directory);
    if (m_directory == target && m_watchDescriptor >= 0)
        return true;

    clearWatch();

    QByteArray path;
    QByteArray nameFilter;
    QVector<QByteArray> extraPaths;
    bool trashRoot = false;
    if (trash) {
        TrashService::Location location;
        if (!TrashService::resolve(directory, &location))
            return false;
        if (location.root) {
            const QVector<QByteArray> paths = TrashService::watchPaths();
            if (paths.isEmpty())
                return false;
            path = paths.first();
            extraPaths = paths.mid(1);
            trashRoot = true;
        } else {
            path = location.realPath;
        }
    } else if (recent) {
        const QFileInfo info(RecentFiles::defaultFilePath());
        path = QFile::encodeName(info.absolutePath());
        nameFilter = QFile::encodeName(info.fileName());
    } else {
        path = PathUtils::toLocalBytes(target);
    }

    const int descriptor = ::inotify_add_watch(m_inotifyFd, path.constData(), kWatchMask);
    if (descriptor < 0) {
        qCWarning(lcWatch) << "No se pudo vigilar" << path << std::strerror(errno);
        return false;
    }

    m_directory = target;
    m_nameFilter = nameFilter;
    m_trashRoot = trashRoot;
    m_watchDescriptor = descriptor;
    for (const QByteArray &extra : std::as_const(extraPaths)) {
        const int extraDescriptor = ::inotify_add_watch(m_inotifyFd, extra.constData(), kWatchMask);
        if (extraDescriptor >= 0)
            m_extraDescriptors.append(extraDescriptor);
    }
    m_notifier->setEnabled(true);
    return true;
}

void DirectoryWatcher::unwatch()
{
    clearWatch();
}

void DirectoryWatcher::onActivated()
{
    if (m_inotifyFd < 0)
        return;

    alignas(struct inotify_event) char buffer[64 * 1024];

    for (;;) {
        const ssize_t length = ::read(m_inotifyFd, buffer, sizeof(buffer));
        if (length < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (length == 0)
            break;

        bool structural = false;
        bool gone = false;

        qsizetype offset = 0;
        while (offset < length) {
            const auto *event = reinterpret_cast<const struct inotify_event *>(buffer + offset);
            offset += qsizetype(sizeof(struct inotify_event) + event->len);

            if (event->mask & IN_IGNORED) {
                if (event->wd == m_watchDescriptor) {
                    m_watchDescriptor = -1;
                    m_notifier->setEnabled(m_trashRoot && !m_extraDescriptors.isEmpty());
                    if (m_trashRoot && !m_extraDescriptors.isEmpty())
                        structural = true;
                } else {
                    m_extraDescriptors.removeAll(event->wd);
                    if (m_trashRoot)
                        structural = true;
                }
                continue;
            }

            if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT)) {
                gone = true;
                continue;
            }

            if (event->mask & (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO
                               | IN_ATTRIB | IN_CLOSE_WRITE | IN_Q_OVERFLOW)) {
                if (m_nameFilter.isEmpty() || (event->mask & IN_Q_OVERFLOW)
                    || (event->len > 0 && m_nameFilter == event->name)) {
                    structural = true;
                }
            }
        }

        if (gone) {
            const bool silent = !m_nameFilter.isEmpty() || m_trashRoot;
            clearWatch();
            if (!silent)
                emit directoryGone();
            return;
        }

        if (structural)
            m_debounce->start();
    }
}

void DirectoryWatcher::clearWatch()
{
    if (m_debounce)
        m_debounce->stop();

    if (m_notifier)
        m_notifier->setEnabled(false);

    if (m_watchDescriptor >= 0 && m_inotifyFd >= 0)
        ::inotify_rm_watch(m_inotifyFd, m_watchDescriptor);

    if (m_inotifyFd >= 0) {
        for (const int extraDescriptor : std::as_const(m_extraDescriptors))
            ::inotify_rm_watch(m_inotifyFd, extraDescriptor);
    }
    m_extraDescriptors.clear();
    m_trashRoot = false;

    m_watchDescriptor = -1;
    m_directory.clear();
    m_nameFilter.clear();

    if (m_inotifyFd >= 0) {
        char buffer[4096];
        while (::read(m_inotifyFd, buffer, sizeof(buffer)) > 0) {
        }
    }
}
