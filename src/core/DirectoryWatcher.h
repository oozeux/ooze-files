#pragma once

#include <QByteArray>
#include <QObject>
#include <QUrl>
#include <QVector>

class QSocketNotifier;
class QTimer;

class DirectoryWatcher : public QObject
{
    Q_OBJECT
public:
    explicit DirectoryWatcher(QObject *parent = nullptr);
    ~DirectoryWatcher() override;

    bool watch(const QUrl &directory);
    void unwatch();

    [[nodiscard]] QUrl directory() const { return m_directory; }
    [[nodiscard]] bool isWatching() const { return m_watchDescriptor >= 0; }

signals:
    void changed();
    void directoryGone();

private:
    void onActivated();
    void clearWatch();

    QUrl m_directory;
    QByteArray m_nameFilter;
    QVector<int> m_extraDescriptors;
    bool m_trashRoot = false;
    int m_inotifyFd = -1;
    int m_watchDescriptor = -1;
    QSocketNotifier *m_notifier = nullptr;
    QTimer *m_debounce = nullptr;
};
