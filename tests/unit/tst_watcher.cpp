#include "core/DirectoryWatcher.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

class TestDirectoryWatcher : public QObject
{
    Q_OBJECT

private slots:
    void reportsChanges();
    void stopsAfterUnwatch();
};

void TestDirectoryWatcher::reportsChanges()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    DirectoryWatcher watcher;
    QSignalSpy changed(&watcher, &DirectoryWatcher::changed);

    QVERIFY(watcher.watch(QUrl::fromLocalFile(temp.path())));
    QVERIFY(watcher.isWatching());

    QFile file(temp.filePath(QStringLiteral("nuevo.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();

    QVERIFY(changed.wait(4000));
    QVERIFY(changed.count() >= 1);
}

void TestDirectoryWatcher::stopsAfterUnwatch()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    DirectoryWatcher watcher;
    QSignalSpy changed(&watcher, &DirectoryWatcher::changed);

    QVERIFY(watcher.watch(QUrl::fromLocalFile(temp.path())));
    watcher.unwatch();
    QVERIFY(!watcher.isWatching());

    QFile file(temp.filePath(QStringLiteral("ignorado.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();

    QTest::qWait(400);
    QCOMPARE(changed.count(), 0);
}

QTEST_MAIN(TestDirectoryWatcher)
#include "tst_watcher.moc"
