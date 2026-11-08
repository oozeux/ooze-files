#include "app/ApplicationController.h"
#include "app/TabListModel.h"
#include "settings/Config.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

namespace {
QString configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/ooze-files/config.json");
}

void createFile(const QString &path, const QByteArray &content = QByteArrayLiteral("x"))
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
    file.close();
}

bool waitForRows(ApplicationController &controller, int expected, int timeout = 5000)
{
    return QTest::qWaitFor([&controller, expected] {
        return !controller.loading() && controller.files()
            && controller.files()->rowCount() == expected;
    }, timeout);
}

QString tabUrl(ApplicationController &controller, int index)
{
    if (!controller.tabs() || index < 0 || index >= controller.tabs()->rowCount())
        return {};
    return controller.tabs()->index(index, 0).data(TabListModel::UrlRole).toString();
}
}

class TestTabs : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void newTabUsesCurrentDirectory();
    void openInNewTabKeepsOtherTabs();
    void activateTabRestoresIndependentState();
    void closeTabActivatesNeighbor();
    void lastTabRequestsWindowClose();
    void duplicateTabInsertsAfterSource();
    void reopenClosedTabRestoresPosition();
    void maxTabsIsEnforced();
    void tabBarModelTracksTabs();
    void openInNewTabSelectsFile();
    void sessionPersistsAndRestores();
    void sessionRestoreSkipsMissingDirectories();
    void explicitTargetSkipsSessionRestore();
};

void TestTabs::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestTabs::init()
{
    QFile::remove(configPath());
    QDir().mkpath(QFileInfo(configPath()).absolutePath());
}

void TestTabs::cleanupTestCase()
{
    QFile::remove(configPath());
}

void TestTabs::newTabUsesCurrentDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));
    createFile(dir.filePath(QStringLiteral("b.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(dir.path()));
    QVERIFY(waitForRows(controller, 2));

    QSignalSpy activeSpy(&controller, &ApplicationController::activeTabChanged);

    controller.newTab();

    QCOMPARE(controller.tabCount(), 2);
    QCOMPARE(controller.activeTab(), 1);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(dir.path()));
    QCOMPARE(activeSpy.count(), 1);
    QVERIFY(waitForRows(controller, 2));
}

void TestTabs::openInNewTabKeepsOtherTabs()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    createFile(first.filePath(QStringLiteral("a.txt")));
    createFile(second.filePath(QStringLiteral("b.txt")));
    createFile(second.filePath(QStringLiteral("c.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(first.path()));
    QVERIFY(waitForRows(controller, 1));

    controller.openInNewTab(QUrl::fromLocalFile(second.path()).toString());
    QCOMPARE(controller.tabCount(), 2);
    QCOMPARE(controller.activeTab(), 1);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(second.path()));
    QVERIFY(waitForRows(controller, 2));

    QCOMPARE(tabUrl(controller, 0), QUrl::fromLocalFile(first.path()).toString());
    QCOMPARE(tabUrl(controller, 1), QUrl::fromLocalFile(second.path()).toString());

    controller.activateTab(0);
    QCOMPARE(controller.activeTab(), 0);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(first.path()));
    QVERIFY(waitForRows(controller, 1));
}

void TestTabs::activateTabRestoresIndependentState()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    createFile(first.filePath(QStringLiteral("a.txt")));
    createFile(first.filePath(QStringLiteral("b.txt")));
    createFile(second.filePath(QStringLiteral("c.txt")));
    createFile(second.filePath(QStringLiteral("d.txt")));
    createFile(second.filePath(QStringLiteral("e.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(first.path()));
    QVERIFY(waitForRows(controller, 2));

    controller.selectAll();
    QCOMPARE(controller.selectionCount(), 2);

    controller.openInNewTab(QUrl::fromLocalFile(second.path()).toString());
    QVERIFY(waitForRows(controller, 3));
    QCOMPARE(controller.selectionCount(), 0);

    controller.selectAll();
    QCOMPARE(controller.selectionCount(), 3);

    controller.activateTab(0);
    QVERIFY(waitForRows(controller, 2));
    QCOMPARE(controller.selectionCount(), 2);
    QCOMPARE(controller.itemCount(), 2);

    controller.activateTab(1);
    QVERIFY(waitForRows(controller, 3));
    QCOMPARE(controller.selectionCount(), 3);
    QCOMPARE(controller.itemCount(), 3);
}

void TestTabs::closeTabActivatesNeighbor()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QTemporaryDir third;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QVERIFY(third.isValid());
    createFile(first.filePath(QStringLiteral("a.txt")));
    createFile(second.filePath(QStringLiteral("b.txt")));
    createFile(third.filePath(QStringLiteral("c.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(first.path()));
    QVERIFY(waitForRows(controller, 1));
    controller.openInNewTab(QUrl::fromLocalFile(second.path()).toString());
    QVERIFY(waitForRows(controller, 1));
    controller.openInNewTab(QUrl::fromLocalFile(third.path()).toString());
    QVERIFY(waitForRows(controller, 1));
    QCOMPARE(controller.activeTab(), 2);

    controller.closeTab(2);
    QCOMPARE(controller.tabCount(), 2);
    QCOMPARE(controller.activeTab(), 1);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(second.path()));

    controller.closeTab(0);
    QCOMPARE(controller.tabCount(), 1);
    QCOMPARE(controller.activeTab(), 0);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(second.path()));
}

void TestTabs::lastTabRequestsWindowClose()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(dir.path()));
    QVERIFY(waitForRows(controller, 1));

    QSignalSpy closeSpy(&controller, &ApplicationController::closeWindowRequested);
    controller.closeTab(0);

    QCOMPARE(closeSpy.count(), 1);
    QCOMPARE(controller.tabCount(), 1);
    QCOMPARE(controller.activeTab(), 0);
}

void TestTabs::duplicateTabInsertsAfterSource()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    createFile(first.filePath(QStringLiteral("a.txt")));
    createFile(second.filePath(QStringLiteral("b.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(first.path()));
    QVERIFY(waitForRows(controller, 1));
    controller.openInNewTab(QUrl::fromLocalFile(second.path()).toString());
    QVERIFY(waitForRows(controller, 1));
    QCOMPARE(controller.activeTab(), 1);

    controller.duplicateTab(0);
    QCOMPARE(controller.tabCount(), 3);
    QCOMPARE(controller.activeTab(), 1);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(first.path()));
    QCOMPARE(tabUrl(controller, 0), QUrl::fromLocalFile(first.path()).toString());
    QCOMPARE(tabUrl(controller, 1), QUrl::fromLocalFile(first.path()).toString());
    QCOMPARE(tabUrl(controller, 2), QUrl::fromLocalFile(second.path()).toString());
    QVERIFY(waitForRows(controller, 1));
}

void TestTabs::reopenClosedTabRestoresPosition()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QTemporaryDir third;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QVERIFY(third.isValid());
    createFile(first.filePath(QStringLiteral("a.txt")));
    createFile(second.filePath(QStringLiteral("b.txt")));
    createFile(third.filePath(QStringLiteral("c.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(first.path()));
    QVERIFY(waitForRows(controller, 1));
    controller.openInNewTab(QUrl::fromLocalFile(second.path()).toString());
    QVERIFY(waitForRows(controller, 1));
    controller.openInNewTab(QUrl::fromLocalFile(third.path()).toString());
    QVERIFY(waitForRows(controller, 1));
    QCOMPARE(controller.activeTab(), 2);

    controller.closeTab(1);
    QCOMPARE(controller.tabCount(), 2);
    QCOMPARE(controller.activeTab(), 1);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(third.path()));
    QVERIFY(controller.canReopenTab());

    controller.reopenClosedTab();
    QCOMPARE(controller.tabCount(), 3);
    QCOMPARE(controller.activeTab(), 1);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(second.path()));
    QCOMPARE(tabUrl(controller, 0), QUrl::fromLocalFile(first.path()).toString());
    QCOMPARE(tabUrl(controller, 1), QUrl::fromLocalFile(second.path()).toString());
    QCOMPARE(tabUrl(controller, 2), QUrl::fromLocalFile(third.path()).toString());
    QVERIFY(waitForRows(controller, 1));
}

void TestTabs::maxTabsIsEnforced()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));

    Config config;
    QVERIFY(config.load());
    config.setMaxTabs(2);

    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(dir.path()));
    QVERIFY(waitForRows(controller, 1));

    controller.newTab();
    QCOMPARE(controller.tabCount(), 2);
    QVERIFY(waitForRows(controller, 1));

    controller.newTab();
    QCOMPARE(controller.tabCount(), 2);
    QVERIFY(controller.statusMessage().contains(QStringLiteral("Máximo de 2")));
}

void TestTabs::tabBarModelTracksTabs()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    const QString subdir = second.filePath(QStringLiteral("sub"));
    QVERIFY(QDir().mkpath(subdir));
    createFile(first.filePath(QStringLiteral("a.txt")));
    createFile(subdir + QStringLiteral("/b.txt"));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(first.path()));
    QVERIFY(waitForRows(controller, 1));
    controller.openInNewTab(QUrl::fromLocalFile(second.path()).toString());
    QVERIFY(waitForRows(controller, 1));

    QAbstractItemModel *tabs = controller.tabs();
    QVERIFY(tabs);
    QCOMPARE(tabs->rowCount(), 2);

    QCOMPARE(tabs->index(0, 0).data(TabListModel::TitleRole).toString(),
             QFileInfo(first.path()).fileName());
    QCOMPARE(tabs->index(1, 0).data(TabListModel::TitleRole).toString(),
             QFileInfo(second.path()).fileName());
    QCOMPARE(tabs->index(0, 0).data(TabListModel::ActiveRole).toBool(), false);
    QCOMPARE(tabs->index(1, 0).data(TabListModel::ActiveRole).toBool(), true);

    controller.goToUrl(QUrl::fromLocalFile(subdir));
    QVERIFY(waitForRows(controller, 1));
    QCOMPARE(tabs->index(1, 0).data(TabListModel::TitleRole).toString(),
             QStringLiteral("sub"));
}

void TestTabs::openInNewTabSelectsFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("objetivo.txt"));
    createFile(file);

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(dir.path()));
    QVERIFY(waitForRows(controller, 1));

    controller.openInNewTab(QUrl::fromLocalFile(file).toString());
    QCOMPARE(controller.tabCount(), 2);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(dir.path()));
    QVERIFY(waitForRows(controller, 1));

    QTRY_VERIFY_WITH_TIMEOUT(controller.selectionCount() == 1, 5000);
    QCOMPARE(controller.selectedName(), QStringLiteral("objetivo.txt"));
}

void TestTabs::sessionPersistsAndRestores()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    createFile(first.filePath(QStringLiteral("a.txt")));
    createFile(second.filePath(QStringLiteral("b.txt")));

    {
        Config config;
        QVERIFY(config.load());
        ApplicationController controller(&config);
        controller.start(QUrl::fromLocalFile(first.path()));
        QVERIFY(waitForRows(controller, 1));
        controller.openInNewTab(QUrl::fromLocalFile(second.path()).toString());
        QVERIFY(waitForRows(controller, 1));
        controller.flushSession();

        QCOMPARE(config.sessionTabs().size(), 2);
        QCOMPARE(config.sessionActiveTab(), 1);
    }

    Config restoredConfig;
    QVERIFY(restoredConfig.load());
    QCOMPARE(restoredConfig.sessionTabs().size(), 2);
    QCOMPARE(restoredConfig.sessionActiveTab(), 1);

    ApplicationController restored(&restoredConfig);
    restored.start(QUrl());
    QCOMPARE(restored.tabCount(), 2);
    QCOMPARE(restored.activeTab(), 1);
    QCOMPARE(restored.currentUrl(), QUrl::fromLocalFile(second.path()));
    QVERIFY(waitForRows(restored, 1));
    QCOMPARE(tabUrl(restored, 0), QUrl::fromLocalFile(first.path()).toString());
    QCOMPARE(tabUrl(restored, 1), QUrl::fromLocalFile(second.path()).toString());
}

void TestTabs::sessionRestoreSkipsMissingDirectories()
{
    QTemporaryDir existing;
    QVERIFY(existing.isValid());
    createFile(existing.filePath(QStringLiteral("a.txt")));
    const QString missing = existing.filePath(QStringLiteral("no-existe"));

    Config writer;
    QVERIFY(writer.load());
    writer.setSession({QUrl::fromLocalFile(existing.path()).toString(),
                       QUrl::fromLocalFile(missing).toString()}, 1);
    QVERIFY(writer.save());

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl());

    QCOMPARE(controller.tabCount(), 1);
    QCOMPARE(controller.activeTab(), 0);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(existing.path()));
    QVERIFY(waitForRows(controller, 1));
}

void TestTabs::explicitTargetSkipsSessionRestore()
{
    QTemporaryDir sessionDir;
    QTemporaryDir targetDir;
    QVERIFY(sessionDir.isValid());
    QVERIFY(targetDir.isValid());
    createFile(sessionDir.filePath(QStringLiteral("a.txt")));
    createFile(targetDir.filePath(QStringLiteral("b.txt")));

    Config writer;
    QVERIFY(writer.load());
    writer.setSession({QUrl::fromLocalFile(sessionDir.path()).toString()}, 0);
    QVERIFY(writer.save());

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    controller.start(QUrl::fromLocalFile(targetDir.path()), {}, true);

    QCOMPARE(controller.tabCount(), 1);
    QCOMPARE(controller.currentUrl(), QUrl::fromLocalFile(targetDir.path()));
    QVERIFY(waitForRows(controller, 1));
}

QTEST_MAIN(TestTabs)
#include "tst_tabs.moc"
