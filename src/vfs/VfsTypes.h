#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QUrl>
#include <QVector>

#include <cerrno>
#include <cstring>
#include <QCoreApplication>

enum class EntryKind : quint8 {
    Unknown = 0,
    File,
    Directory,
    Symlink,
    Socket,
    Fifo,
    BlockDevice,
    CharDevice,
};

struct VfsEntry
{
    QByteArray id;
    QByteArray path;
    QUrl url;
    QString displayName;
    EntryKind kind = EntryKind::Unknown;
    qint64 size = -1;
    QDateTime modified;
    QString mimeType;
    QString iconName;
    QString suffix;
    quint32 flags = 0;
    QString location;
    QDateTime activity;
};

struct VfsStat
{
    int index = -1;
    qint64 size = -1;
    qint64 mtimeSecs = 0;
    QString mimeType;
    QString iconName;
    quint32 flags = 0;
};

enum VfsFlag : quint32 {
    FlagHidden = 1u << 0,
    FlagReadable = 1u << 1,
    FlagWritable = 1u << 2,
    FlagExecutable = 1u << 3,
    FlagSymlinkBroken = 1u << 4,
    FlagMountPoint = 1u << 5,
    FlagSymlinkDir = 1u << 6,
};

struct VfsCapabilities
{
    bool canList = false;
    bool canRead = false;
    bool canWrite = false;
    bool canDelete = false;
    bool canRename = false;
    bool canCreateDirectory = false;
    bool canWatch = false;
    bool canSearch = false;
    bool supportsTrash = false;
};

struct VfsError
{
    enum Code : quint8 {
        None = 0,
        NotFound,
        PermissionDenied,
        NotADirectory,
        IoError,
        Unsupported,
        Cancelled,
        Unknown,
    };

    Code code = None;
    QUrl url;
    QString message;

    [[nodiscard]] bool isValid() const { return code != None; }
    [[nodiscard]] bool isCancelled() const { return code == Cancelled; }

    static VfsError fromErrno(int err, const QUrl &url)
    {
        VfsError e;
        e.url = url;
        switch (err) {
        case 0:
            break;
        case ENOENT:
            e.code = NotFound;
            e.message = QCoreApplication::translate("Ooze", "No existe");
            break;
        case EACCES:
        case EPERM:
            e.code = PermissionDenied;
            e.message = QCoreApplication::translate("Ooze", "Permiso denegado");
            break;
        case ENOTDIR:
            e.code = NotADirectory;
            e.message = QCoreApplication::translate("Ooze", "No es un directorio");
            break;
        default:
            e.code = IoError;
            e.message = QString::fromLocal8Bit(strerror(err));
            break;
        }
        return e;
    }
};

inline bool hasFlag(const VfsEntry &entry, VfsFlag flag)
{
    return (entry.flags & static_cast<quint32>(flag)) != 0;
}

Q_DECLARE_METATYPE(VfsEntry)
Q_DECLARE_METATYPE(VfsStat)
Q_DECLARE_METATYPE(VfsError)
Q_DECLARE_METATYPE(QVector<VfsEntry>)
Q_DECLARE_METATYPE(QVector<VfsStat>)
