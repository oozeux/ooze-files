#pragma once

#include <QObject>
#include <QUrl>

class SingleInstance : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.ooze.Files")

public:
    enum class Mode {
        Open,
        NewTab,
        NewWindow,
    };

    explicit SingleInstance(QObject *parent = nullptr);

    bool tryAcquire();
    bool notifyExisting(const QUrl &url, const QString &selectName, Mode mode);

    [[nodiscard]] bool isOwner() const { return m_owner; }
    [[nodiscard]] static QString serviceName();
    [[nodiscard]] static QString objectPath();

signals:
    void openRequested(const QUrl &url, bool select);
    void newTabRequested(const QUrl &url);
    void newWindowRequested(const QUrl &url);
    void toggleRequested();

public slots:
    Q_SCRIPTABLE void Open(const QString &url);
    Q_SCRIPTABLE void OpenAndSelect(const QString &url);
    Q_SCRIPTABLE void NewTab(const QString &url);
    Q_SCRIPTABLE void NewWindow(const QString &url);
    Q_SCRIPTABLE void Toggle();

private:
    bool m_owner = false;
};
