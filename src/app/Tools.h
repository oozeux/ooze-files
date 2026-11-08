#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

// Resolves the optional external tools once at startup so QML can explain why
// thumbnails or previews are unavailable instead of silently showing nothing.
class Tools : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool ffmpeg READ hasFfmpeg NOTIFY changed)
    Q_PROPERTY(bool ffprobe READ hasFfprobe NOTIFY changed)
    Q_PROPERTY(bool unsquashfs READ hasUnsquashfs NOTIFY changed)
    Q_PROPERTY(bool pdftoppm READ hasPdftoppm NOTIFY changed)
    Q_PROPERTY(QVariantList optional READ optional NOTIFY changed)
    Q_PROPERTY(bool anyMissing READ anyMissing NOTIFY changed)

public:
    explicit Tools(QObject *parent = nullptr);

    [[nodiscard]] bool hasFfmpeg() const;
    [[nodiscard]] bool hasFfprobe() const;
    [[nodiscard]] bool hasUnsquashfs() const;
    [[nodiscard]] bool hasPdftoppm() const;
    [[nodiscard]] bool anyMissing() const;

    [[nodiscard]] QVariantList optional() const;

    Q_INVOKABLE bool has(const QString &id) const;
    Q_INVOKABLE QString path(const QString &id) const;
    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    void resolve();

    QString m_ffmpeg;
    QString m_ffprobe;
    QString m_unsquashfs;
    QString m_pdftoppm;
};
