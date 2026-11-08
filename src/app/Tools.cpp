#include "app/Tools.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariantMap>
#include <QCoreApplication>

namespace {

QString resolveExecutable(const QString &name, const QString &environmentOverride = {})
{
    if (!environmentOverride.isEmpty()) {
        const QString fromEnv = qEnvironmentVariable(environmentOverride.toUtf8().constData());
        if (!fromEnv.isEmpty() && QFileInfo(fromEnv).isExecutable())
            return fromEnv;
    }

    const QString fromPath = QStandardPaths::findExecutable(name);
    if (!fromPath.isEmpty())
        return fromPath;

    const QStringList fallbacks = {
        QStringLiteral("/run/current-system/sw/bin/") + name,
        QStringLiteral("/usr/bin/") + name,
        QStringLiteral("/usr/local/bin/") + name,
        QStringLiteral("/usr/sbin/") + name,
        QStringLiteral("/usr/local/sbin/") + name,
        QDir::homePath() + QStringLiteral("/.nix-profile/bin/") + name,
    };
    for (const QString &candidate : fallbacks) {
        if (QFileInfo(candidate).isExecutable())
            return candidate;
    }

    return {};
}

} // namespace

Tools::Tools(QObject *parent)
    : QObject(parent)
{
    resolve();
}

void Tools::resolve()
{
    m_ffmpeg = resolveExecutable(QStringLiteral("ffmpeg"));
    m_ffprobe = resolveExecutable(QStringLiteral("ffprobe"));
    m_unsquashfs = resolveExecutable(QStringLiteral("unsquashfs"),
                                     QStringLiteral("OOZE_FILES_UNSQUASHFS"));
    m_pdftoppm = resolveExecutable(QStringLiteral("pdftoppm"));
}

void Tools::refresh()
{
    const QString ffmpeg = m_ffmpeg;
    const QString ffprobe = m_ffprobe;
    const QString unsquashfs = m_unsquashfs;
    const QString pdftoppm = m_pdftoppm;

    resolve();

    if (ffmpeg != m_ffmpeg || ffprobe != m_ffprobe || unsquashfs != m_unsquashfs
        || pdftoppm != m_pdftoppm) {
        emit changed();
    }
}

bool Tools::hasFfmpeg() const { return !m_ffmpeg.isEmpty(); }
bool Tools::hasFfprobe() const { return !m_ffprobe.isEmpty(); }
bool Tools::hasUnsquashfs() const { return !m_unsquashfs.isEmpty(); }
bool Tools::hasPdftoppm() const { return !m_pdftoppm.isEmpty(); }

bool Tools::anyMissing() const
{
    return m_ffmpeg.isEmpty() || m_ffprobe.isEmpty() || m_unsquashfs.isEmpty()
        || m_pdftoppm.isEmpty();
}

bool Tools::has(const QString &id) const { return !path(id).isEmpty(); }

QString Tools::path(const QString &id) const
{
    if (id == QLatin1String("ffmpeg"))
        return m_ffmpeg;
    if (id == QLatin1String("ffprobe"))
        return m_ffprobe;
    if (id == QLatin1String("unsquashfs"))
        return m_unsquashfs;
    if (id == QLatin1String("pdftoppm"))
        return m_pdftoppm;
    return {};
}

QVariantList Tools::optional() const
{
    const auto entry = [this](const QString &id, const QString &label,
                              const QString &purpose, const QString &package) {
        const QString resolved = path(id);
        QVariantMap map;
        map.insert(QStringLiteral("id"), id);
        map.insert(QStringLiteral("label"), label);
        map.insert(QStringLiteral("purpose"), purpose);
        map.insert(QStringLiteral("package"), package);
        map.insert(QStringLiteral("found"), !resolved.isEmpty());
        map.insert(QStringLiteral("path"), resolved);
        return map;
    };

    return {
        entry(QStringLiteral("ffmpeg"), QStringLiteral("ffmpeg"),
              QCoreApplication::translate("Ooze", "Miniaturas y pósteres de vídeo"),
              QStringLiteral("ffmpeg")),
        entry(QStringLiteral("ffprobe"), QStringLiteral("ffprobe"),
              QCoreApplication::translate("Ooze", "Duración del vídeo para elegir el fotograma"),
              QStringLiteral("ffmpeg")),
        entry(QStringLiteral("unsquashfs"), QStringLiteral("unsquashfs"),
              QCoreApplication::translate("Ooze", "Iconos incrustados en AppImage"),
              QStringLiteral("squashfs-tools")),
        entry(QStringLiteral("pdftoppm"), QStringLiteral("pdftoppm"),
              QCoreApplication::translate("Ooze", "Vista previa de la primera página de PDF"),
              QStringLiteral("poppler-utils")),
    };
}
