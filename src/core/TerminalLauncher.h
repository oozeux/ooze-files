#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class TerminalLauncher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit TerminalLauncher(QObject *parent = nullptr);

    bool openAt(const QUrl &target, const QString &configuredTerminal = {});

    [[nodiscard]] QString lastError() const { return m_lastError; }

    [[nodiscard]] static QString detect(const QString &configuredTerminal = {});

signals:
    void lastErrorChanged();

private:
    QString m_lastError;
};
