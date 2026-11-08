#pragma once

#include <QColor>
#include <QDBusVariant>
#include <QObject>

class QDBusArgument;
class QVariant;

class PortalSettings : public QObject
{
    Q_OBJECT

public:
    explicit PortalSettings(QObject *parent = nullptr);

    [[nodiscard]] QColor accent() const { return m_accent; }
    // 0 = sin preferencia, 1 = oscuro, 2 = claro
    [[nodiscard]] int colorScheme() const { return m_colorScheme; }

signals:
    void accentChanged(const QColor &accent);
    void colorSchemeChanged(int scheme);

private slots:
    void onSettingChanged(const QString &nameSpace, const QString &key,
                          const QDBusVariant &value);

private:
    void read(const QString &key);
    void applyValue(const QString &key, const QVariant &value);

    QColor m_accent;
    int m_colorScheme = 0;
};
