#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class ThemeStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString paletteId READ paletteId NOTIFY paletteChanged)
    Q_PROPERTY(QString paletteName READ paletteName NOTIFY paletteChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantList choices READ choices NOTIFY choicesChanged)

    Q_PROPERTY(QColor bg READ bg NOTIFY paletteChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY paletteChanged)
    Q_PROPERTY(QColor surfaceAlt READ surfaceAlt NOTIFY paletteChanged)
    Q_PROPERTY(QColor surfaceHover READ surfaceHover NOTIFY paletteChanged)
    Q_PROPERTY(QColor surfaceActive READ surfaceActive NOTIFY paletteChanged)
    Q_PROPERTY(QColor border READ border NOTIFY paletteChanged)
    Q_PROPERTY(QColor fg READ fg NOTIFY paletteChanged)
    Q_PROPERTY(QColor fgMuted READ fgMuted NOTIFY paletteChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY paletteChanged)
    Q_PROPERTY(QColor danger READ danger NOTIFY paletteChanged)
    Q_PROPERTY(QColor selectionBg READ selectionBg NOTIFY paletteChanged)

    Q_PROPERTY(int radius READ radius NOTIFY paletteChanged)
    Q_PROPERTY(int radiusSmall READ radiusSmall NOTIFY paletteChanged)
    Q_PROPERTY(int borderWidth READ borderWidth NOTIFY paletteChanged)

public:
    explicit ThemeStore(QObject *parent = nullptr);

    void apply(const QString &paletteId, bool dark);
    void setSystemAccent(const QColor &accent);

    [[nodiscard]] QString paletteId() const { return m_requested; }
    [[nodiscard]] QString paletteName() const { return m_name; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] QVariantList choices() const { return m_choices; }

    [[nodiscard]] QColor bg() const { return m_palette.bg; }
    [[nodiscard]] QColor surface() const { return m_palette.surface; }
    [[nodiscard]] QColor surfaceAlt() const { return m_palette.surfaceAlt; }
    [[nodiscard]] QColor surfaceHover() const { return m_palette.surfaceHover; }
    [[nodiscard]] QColor surfaceActive() const { return m_palette.surfaceActive; }
    [[nodiscard]] QColor border() const { return m_palette.border; }
    [[nodiscard]] QColor fg() const { return m_palette.fg; }
    [[nodiscard]] QColor fgMuted() const { return m_palette.fgMuted; }
    [[nodiscard]] QColor accent() const { return m_palette.accent; }
    [[nodiscard]] QColor danger() const { return m_palette.danger; }
    [[nodiscard]] QColor selectionBg() const { return m_palette.selectionBg; }

    [[nodiscard]] int radius() const { return m_palette.radius; }
    [[nodiscard]] int radiusSmall() const { return m_palette.radiusSmall; }
    [[nodiscard]] int borderWidth() const { return m_palette.borderWidth; }

    [[nodiscard]] static QString themesDirectory();
    [[nodiscard]] static QString oozeShellColorsPath();
    [[nodiscard]] static bool isValidId(const QString &id);
    [[nodiscard]] static bool isValidRef(const QString &ref);

    Q_INVOKABLE void refreshChoices();
    Q_INVOKABLE void reload();

signals:
    void paletteChanged();
    void lastErrorChanged();
    void choicesChanged();

private:
    struct Palette {
        QColor bg;
        QColor surface;
        QColor surfaceAlt;
        QColor surfaceHover;
        QColor surfaceActive;
        QColor border;
        QColor fg;
        QColor fgMuted;
        QColor accent;
        QColor danger;
        QColor selectionBg;
        int radius = 10;
        int radiusSmall = 6;
        int borderWidth = 1;
    };

    static Palette builtinPalette(bool dark);
    static bool loadFile(const QString &path, bool dark, const QString &id,
                         Palette *out, QString *name, QString *error);

    static bool loadOozeShell(bool dark, Palette *out, QString *name, QStringList *sources,
                              QString *error);
    static bool loadQt6ct(bool dark, Palette *out, QString *name, QStringList *sources,
                          QString *error);
    static bool loadKde(bool dark, Palette *out, QString *name, QStringList *sources,
                        QString *error);

    void setError(const QString &error);
    void updateWatchPaths(const QStringList &sources);

    Palette m_palette;
    QString m_requested = QStringLiteral("builtin");
    QString m_name = QStringLiteral("Incorporado");
    QString m_lastError;
    QVariantList m_choices;
    bool m_dark = false;
    QColor m_systemAccent;

    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
};
