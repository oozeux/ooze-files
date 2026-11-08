#include "core/Archive.h"

#include <QStandardPaths>
#include <QVariantMap>
#include <QCoreApplication>

namespace Archive {

namespace {

struct Suffix
{
    const char *text;
    const char *kind;
};

const Suffix kSuffixes[] = {
    {".tar.gz", "tar"},  {".tar.xz", "tar"},  {".tar.zst", "tar"}, {".tar.bz2", "tar"},
    {".tgz", "tar"},     {".txz", "tar"},     {".tzst", "tar"},     {".tbz2", "tar"},
    {".tar", "tar"},     {".zip", "zip"},     {".7z", "7z"},        {".rar", "rar"},
};

struct Format
{
    const char *id;
    const char *suffix;
    const char *label;
};

const Format kFormats[] = {
    {"zip", ".zip", "ZIP (.zip)"},
    {"tar.gz", ".tar.gz", "TAR + gzip (.tar.gz)"},
    {"tar.xz", ".tar.xz", "TAR + xz (.tar.xz)"},
    {"tar.zst", ".tar.zst", "TAR + zstd (.tar.zst)"},
    {"7z", ".7z", "7-Zip (.7z)"},
};

QString find(const QStringList &names)
{
    for (const QString &name : names) {
        const QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty())
            return path;
    }
    return {};
}

QString sevenZip()
{
    return find({QStringLiteral("7zz"), QStringLiteral("7z"), QStringLiteral("7za"),
                 QStringLiteral("7zr")});
}

QString tarTool()
{
    return find({QStringLiteral("tar"), QStringLiteral("bsdtar")});
}

QString kindFor(const QString &fileName)
{
    const QString lower = fileName.toLower();
    for (const Suffix &suffix : kSuffixes) {
        if (lower.endsWith(QLatin1String(suffix.text)))
            return QLatin1String(suffix.kind);
    }
    return {};
}

QStringList safeNames(const QStringList &names)
{
    QStringList result;
    for (const QString &name : names)
        result.append(name.startsWith(QLatin1Char('-')) ? QStringLiteral("./") + name : name);
    return result;
}

bool toolAvailable(const QString &formatId)
{
    if (formatId == QLatin1String("zip")) {
        return !find({QStringLiteral("zip")}).isEmpty() || !sevenZip().isEmpty()
            || !find({QStringLiteral("bsdtar")}).isEmpty();
    }

    if (formatId == QLatin1String("7z"))
        return !sevenZip().isEmpty();

    if (formatId == QLatin1String("tar.gz"))
        return !tarTool().isEmpty() && !find({QStringLiteral("gzip")}).isEmpty();

    if (formatId == QLatin1String("tar.xz"))
        return !tarTool().isEmpty() && !find({QStringLiteral("xz")}).isEmpty();

    if (formatId == QLatin1String("tar.zst"))
        return !tarTool().isEmpty() && !find({QStringLiteral("zstd")}).isEmpty();

    return false;
}

}

bool isArchiveName(const QString &fileName)
{
    return !kindFor(fileName).isEmpty();
}

QString baseNameWithoutSuffix(const QString &fileName)
{
    const QString lower = fileName.toLower();
    for (const Suffix &suffix : kSuffixes) {
        const QString text = QLatin1String(suffix.text);
        if (lower.endsWith(text) && fileName.size() > text.size())
            return fileName.left(fileName.size() - text.size());
    }
    return fileName;
}

QString suffixForFormat(const QString &formatId)
{
    for (const Format &format : kFormats) {
        if (formatId == QLatin1String(format.id))
            return QLatin1String(format.suffix);
    }
    return {};
}

QVariantList availableFormats()
{
    QVariantList list;
    for (const Format &format : kFormats) {
        const QString id = QLatin1String(format.id);
        if (!toolAvailable(id))
            continue;

        QVariantMap entry;
        entry.insert(QStringLiteral("id"), id);
        entry.insert(QStringLiteral("label"), QString::fromUtf8(format.label));
        entry.insert(QStringLiteral("suffix"), QLatin1String(format.suffix));
        list.append(entry);
    }
    return list;
}

Command compressCommand(const QString &formatId, const QString &archivePath,
                        const QStringList &names, const QString &workingDirectory,
                        QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return Command();
    };

    if (names.isEmpty())
        return fail(QCoreApplication::translate("Ooze", "No hay nada que comprimir"));

    Command command;
    command.workingDirectory = workingDirectory;
    const QStringList items = safeNames(names);

    if (formatId == QLatin1String("zip")) {
        if (const QString zip = find({QStringLiteral("zip")}); !zip.isEmpty()) {
            command.program = zip;
            command.arguments = QStringList{QStringLiteral("-r"), QStringLiteral("-q"),
                                            archivePath} + items;
        } else if (const QString seven = sevenZip(); !seven.isEmpty()) {
            command.program = seven;
            command.arguments = QStringList{QStringLiteral("a"), QStringLiteral("-tzip"),
                                            QStringLiteral("-bso0"), archivePath} + items;
        } else if (const QString bsd = find({QStringLiteral("bsdtar")}); !bsd.isEmpty()) {
            command.program = bsd;
            command.arguments = QStringList{QStringLiteral("--format"), QStringLiteral("zip"),
                                            QStringLiteral("-cf"), archivePath} + items;
        }
    } else if (formatId == QLatin1String("7z")) {
        if (const QString seven = sevenZip(); !seven.isEmpty()) {
            command.program = seven;
            command.arguments = QStringList{QStringLiteral("a"), QStringLiteral("-bso0"),
                                            archivePath} + items;
        }
    } else if (formatId == QLatin1String("tar.gz") || formatId == QLatin1String("tar.xz")
               || formatId == QLatin1String("tar.zst")) {
        const QString tar = tarTool();
        if (!tar.isEmpty()) {
            const QString flag = formatId == QLatin1String("tar.gz") ? QStringLiteral("-z")
                : formatId == QLatin1String("tar.xz")                ? QStringLiteral("-J")
                                                                     : QStringLiteral("--zstd");
            command.program = tar;
            command.arguments = QStringList{flag, QStringLiteral("-cf"), archivePath,
                                            QStringLiteral("--")} + items;
        }
    } else {
        return fail(QCoreApplication::translate("Ooze", "Formato de archivo no admitido"));
    }

    if (!command.isValid())
        return fail(QCoreApplication::translate("Ooze", "No hay ninguna herramienta instalada para crear este formato"));

    return command;
}

Command extractCommand(const QString &archivePath, const QString &destination, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return Command();
    };

    const QString kind = kindFor(archivePath);
    if (kind.isEmpty())
        return fail(QCoreApplication::translate("Ooze", "No es un archivo comprimido conocido"));

    Command command;
    command.workingDirectory = destination;

    const QString bsd = find({QStringLiteral("bsdtar")});
    const QString seven = sevenZip();

    if (kind == QLatin1String("tar")) {
        const QString tar = tarTool();
        if (!tar.isEmpty()) {
            command.program = tar;
            command.arguments = {QStringLiteral("-xf"), archivePath, QStringLiteral("-C"),
                                 destination};
        }
    } else if (kind == QLatin1String("zip")) {
        if (const QString unzip = find({QStringLiteral("unzip")}); !unzip.isEmpty()) {
            command.program = unzip;
            command.arguments = {QStringLiteral("-q"), QStringLiteral("-o"), archivePath,
                                 QStringLiteral("-d"), destination};
        } else if (!bsd.isEmpty()) {
            command.program = bsd;
            command.arguments = {QStringLiteral("-xf"), archivePath, QStringLiteral("-C"),
                                 destination};
        } else if (!seven.isEmpty()) {
            command.program = seven;
            command.arguments = {QStringLiteral("x"), QStringLiteral("-y"),
                                 QStringLiteral("-bso0"), QStringLiteral("-o") + destination,
                                 archivePath};
        }
    } else if (kind == QLatin1String("7z")) {
        if (!seven.isEmpty()) {
            command.program = seven;
            command.arguments = {QStringLiteral("x"), QStringLiteral("-y"),
                                 QStringLiteral("-bso0"), QStringLiteral("-o") + destination,
                                 archivePath};
        } else if (!bsd.isEmpty()) {
            command.program = bsd;
            command.arguments = {QStringLiteral("-xf"), archivePath, QStringLiteral("-C"),
                                 destination};
        }
    } else if (kind == QLatin1String("rar")) {
        if (const QString unrar = find({QStringLiteral("unrar")}); !unrar.isEmpty()) {
            command.program = unrar;
            command.arguments = {QStringLiteral("x"), QStringLiteral("-y"), QStringLiteral("-idq"),
                                 archivePath, destination + QLatin1Char('/')};
        } else if (!seven.isEmpty()) {
            command.program = seven;
            command.arguments = {QStringLiteral("x"), QStringLiteral("-y"),
                                 QStringLiteral("-bso0"), QStringLiteral("-o") + destination,
                                 archivePath};
        } else if (!bsd.isEmpty()) {
            command.program = bsd;
            command.arguments = {QStringLiteral("-xf"), archivePath, QStringLiteral("-C"),
                                 destination};
        }
    }

    if (!command.isValid())
        return fail(QCoreApplication::translate("Ooze", "No hay ninguna herramienta instalada para extraer este formato"));

    return command;
}

}
