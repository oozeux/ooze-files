#include "app/ApplicationController.h"
#include "app/ActionRegistry.h"
#include "settings/Config.h"
#include "theme/ThemeStore.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

namespace {
QVariantMap actionById(const QVariantList &actions, const QString &id)
{
    for (const QVariant &entry : actions) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("id")).toString() == id)
            return map;

        const QVariantMap nested = actionById(map.value(QStringLiteral("submenu")).toList(), id);
        if (!nested.isEmpty())
            return nested;
    }
    return {};
}

int submenuCount(const QVariantList &menu)
{
    int count = 0;
    for (const QVariant &entry : menu) {
        if (entry.toMap().contains(QStringLiteral("submenu")))
            ++count;
    }
    return count;
}
}

class TestController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void dragUriListUsesSelection();
    void dragUriListFallsBackToRow();
    void dropMovesFiles();
    void dropCopiesFiles();
    void dropLinksFiles();
    void dropIgnoresSameDirectoryMove();
    void dropRejectsRemoteUrls();
    void paletteSwitchesWithConfig();
    void actionsExposeCommandsAndMenu();
    void actionsTriggerControllerState();
    void previewAndOpenWithResolveRows();
    void customPlacesCanBeAddedAndRemoved();
    void placesCanBeReorderedAndPersist();
    void actionsRequestUi();
    void actionsTrackEnabledState();
    void selectRowsReplacesAddsAndToggles();
    void cutClipboardSurvivesUnrelatedOperations();
    void clipboardMarkReflectsCopyAndCut();
    void selectedRowsFollowsOrder();

private:
    void createFile(const QString &path, const QByteArray &content = QByteArrayLiteral("x"));
    void startInDirectory(ApplicationController &controller, const QString &path);
};

void TestController::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestController::createFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
    file.close();
}

void TestController::startInDirectory(ApplicationController &controller, const QString &path)
{
    controller.start(QUrl::fromLocalFile(path));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.files()->rowCount() > 0, 5000);
}

void TestController::dragUriListUsesSelection()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));
    createFile(dir.filePath(QStringLiteral("b.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    controller.selectAll();
    QCOMPARE(controller.selectionCount(), 2);

    const QStringList uris = controller.dragUriList(0).split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
    QCOMPARE(uris.size(), 2);
    QCOMPARE(QSet<QString>(uris.begin(), uris.end()),
             QSet<QString>({QUrl::fromLocalFile(dir.filePath(QStringLiteral("a.txt"))).toString(),
                            QUrl::fromLocalFile(dir.filePath(QStringLiteral("b.txt"))).toString()}));
}

void TestController::dragUriListFallsBackToRow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));
    createFile(dir.filePath(QStringLiteral("b.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    controller.clearSelection();

    const QString uri = controller.dragUriList(0);
    QVERIFY(!uri.isEmpty());
    QVERIFY(!uri.contains(QStringLiteral("\r\n")));

    const QUrl url(uri);
    QVERIFY(url.isLocalFile());
    QCOMPARE(QFileInfo(url.toLocalFile()).fileName().isEmpty(), false);
    QVERIFY(QFileInfo::exists(url.toLocalFile()));
}

void TestController::dropMovesFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("a.txt"));
    const QString targetDir = dir.filePath(QStringLiteral("dest"));
    QVERIFY(QDir().mkpath(targetDir));
    createFile(source);

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    controller.dropUrls({QUrl::fromLocalFile(source)}, Qt::MoveAction,
                        QUrl::fromLocalFile(targetDir).toString());

    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(targetDir + QStringLiteral("/a.txt")), 5000);
    QVERIFY(!QFileInfo::exists(source));
}

void TestController::dropCopiesFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("a.txt"));
    const QString targetDir = dir.filePath(QStringLiteral("dest"));
    QVERIFY(QDir().mkpath(targetDir));
    createFile(source);

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    controller.dropUrls({QUrl::fromLocalFile(source)}, Qt::CopyAction,
                        QUrl::fromLocalFile(targetDir).toString());

    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(targetDir + QStringLiteral("/a.txt")), 5000);
    QVERIFY(QFileInfo::exists(source));
}

void TestController::dropLinksFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("a.txt"));
    const QString targetDir = dir.filePath(QStringLiteral("dest"));
    QVERIFY(QDir().mkpath(targetDir));
    createFile(source);

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    controller.dropUrls({QUrl::fromLocalFile(source)}, Qt::LinkAction,
                        QUrl::fromLocalFile(targetDir).toString());

    const QString link = targetDir + QStringLiteral("/a.txt");
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(link).isSymLink(), 5000);
    QCOMPARE(QFileInfo(link).symLinkTarget(), source);
    QVERIFY(QFileInfo::exists(source));
}

void TestController::dropIgnoresSameDirectoryMove()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("a.txt"));
    createFile(source);

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    controller.dropUrls({QUrl::fromLocalFile(source)}, Qt::MoveAction, QUrl::fromLocalFile(dir.path()).toString());

    QTest::qWait(300);
    QVERIFY(QFileInfo::exists(source));
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("a (1).txt"))));
}

void TestController::dropRejectsRemoteUrls()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    controller.dropUrls({QUrl(QStringLiteral("https://example.invalid/file"))}, Qt::CopyAction,
                        QUrl::fromLocalFile(dir.path()).toString());

    QCOMPARE(controller.statusMessage(), QStringLiteral("Solo se admiten archivos locales"));
}

void TestController::paletteSwitchesWithConfig()
{
    Config config;
    QVERIFY(config.load());
    config.setPaletteId(QStringLiteral("builtin"));
    config.setThemeMode(QStringLiteral("light"));

    ApplicationController controller(&config);
    const QColor builtinBg = controller.themeStore()->bg();
    QCOMPARE(builtinBg, QColor(QStringLiteral("#f5f6f8")));

    const QString themesDir =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/ooze-files/themes");
    QDir().mkpath(themesDir);
    QFile themeFile(themesDir + QStringLiteral("/prueba.json"));
    QVERIFY(themeFile.open(QIODevice::WriteOnly));
    themeFile.write(R"({ "schema": 1, "light": { "bg": "#112233" } })");
    themeFile.close();

    controller.setPaletteId(QStringLiteral("prueba"));
    QCOMPARE(controller.paletteId(), QStringLiteral("prueba"));
    QCOMPARE(controller.themeStore()->bg(), QColor(QStringLiteral("#112233")));
    QVERIFY(controller.themeStore()->lastError().isEmpty());

    controller.setPaletteId(QStringLiteral("builtin"));
    QCOMPARE(controller.themeStore()->bg(), builtinBg);
}

void TestController::actionsExposeCommandsAndMenu()
{
    Config config;
    QVERIFY(config.load());
    config.setIconSize(QStringLiteral("normal"));

    ApplicationController controller(&config);
    ActionRegistry actions(&controller);

    QVERIFY(actions.commands().size() >= 35);
    QVERIFY(!actionById(actions.commands(), QStringLiteral("palette.open")).isEmpty());
    QVERIFY(!actionById(actions.commands(), QStringLiteral("select.all")).isEmpty());

    const QVariantList menu = actions.mainMenu();
    QVERIFY(menu.size() >= 15);
    QCOMPARE(actionById(menu, QStringLiteral("theme.light")).value(QStringLiteral("label")).toString(),
             QStringLiteral("Claro"));
    QVERIFY(submenuCount(menu) >= 6);
    QVERIFY(!actionById(menu, QStringLiteral("icons.huge")).isEmpty());
    QVERIFY(!actionById(menu, QStringLiteral("sort.descending")).isEmpty());
    QVERIFY(!actionById(menu, QStringLiteral("view.settings")).isEmpty());
    QCOMPARE(actionById(menu, QStringLiteral("icons.normal")).value(QStringLiteral("checked")).toBool(),
             true);
    QVERIFY(!actionById(menu, QStringLiteral("view.thumbnails")).isEmpty());
    QVERIFY(!actionById(menu, QStringLiteral("view.infoPanel")).isEmpty());
    QVERIFY(!actionById(menu, QStringLiteral("edit.undo")).isEmpty());
    QCOMPARE(actionById(menu, QStringLiteral("tools.terminal")).value(QStringLiteral("hint")).toString(),
             QStringLiteral("Shift+F4"));
    QVERIFY(actionById(menu, QStringLiteral("edit.undo")).value(QStringLiteral("label")).toString()
                .startsWith(QStringLiteral("Deshacer")));
    QVERIFY(!actionById(actions.commands(), QStringLiteral("selection.preview")).isEmpty());
    QVERIFY(!actionById(actions.commands(), QStringLiteral("icons.large")).isEmpty());
    QCOMPARE(menu.constLast().toMap().value(QStringLiteral("enabled")).toBool(), false);
    QVERIFY(menu.constLast().toMap().value(QStringLiteral("label")).toString()
                .startsWith(QStringLiteral("Ooze Files")));
}

void TestController::customPlacesCanBeAddedAndRemoved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("nota.txt")), QByteArrayLiteral("x"));
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("proyectos")));

    Config config;
    QVERIFY(config.load());
    const QVariantList leftovers = config.customPlaces();
    for (const QVariant &place : leftovers)
        config.removePlace(place.toMap().value(QStringLiteral("path")).toString());

    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    const auto customCount = [&controller] {
        int count = 0;
        for (const QVariant &place : controller.places()) {
            if (place.toMap().value(QStringLiteral("custom")).toBool())
                ++count;
        }
        return count;
    };

    const int before = customCount();
    QSignalSpy changed(&controller, &ApplicationController::placesChanged);

    const int fileRow = controller.rowForName(QStringLiteral("nota.txt"));
    const int dirRow = controller.rowForName(QStringLiteral("proyectos"));
    QVERIFY(!controller.addPlaceForRow(fileRow));
    QVERIFY(controller.addPlaceForRow(dirRow));
    QCOMPARE(customCount(), before + 1);
    QCOMPARE(changed.count(), 1);

    const QString url = QUrl::fromLocalFile(dir.filePath(QStringLiteral("proyectos"))).toString();
    QVERIFY(controller.isCustomPlace(url));
    QVERIFY(!controller.addPlaceForRow(dirRow));

    QVERIFY(controller.addCurrentFolderToPlaces());
    QCOMPARE(customCount(), before + 2);

    controller.removePlace(url);
    QVERIFY(!controller.isCustomPlace(url));
    QCOMPARE(customCount(), before + 1);

    QVERIFY(!controller.addPlace(QStringLiteral("file:///no/existe/jamas")));
    QVERIFY(!controller.addPlace(QStringLiteral("sftp://host/dir")));
}

void TestController::placesCanBeReorderedAndPersist()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("uno")));
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("dos")));

    Config config;
    QVERIFY(config.load());
    const QVariantList leftovers = config.customPlaces();
    for (const QVariant &place : leftovers)
        config.removePlace(place.toMap().value(QStringLiteral("path")).toString());
    config.setPlacesOrder({});

    const QString uno = dir.filePath(QStringLiteral("uno"));
    const QString dos = dir.filePath(QStringLiteral("dos"));
    QVERIFY(config.addPlace(uno));
    QVERIFY(config.addPlace(dos));

    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    const auto placeIds = [&controller] {
        QStringList ids;
        for (const QVariant &place : controller.places()) {
            const QVariantMap map = place.toMap();
            if (map.value(QStringLiteral("section")).toString() == QLatin1String("places"))
                ids.append(map.value(QStringLiteral("id")).toString());
        }
        return ids;
    };

    QStringList ids = placeIds();
    QCOMPARE(ids.mid(ids.size() - 2), (QStringList{uno, dos}));

    QVERIFY(!controller.movePlace(QStringLiteral("/no/existe"), 0));
    QVERIFY(!controller.movePlace(dos, int(ids.size())));

    QSignalSpy changed(&controller, &ApplicationController::placesChanged);
    QVERIFY(controller.movePlace(dos, 0));
    QCOMPARE(changed.count(), 1);

    ids = placeIds();
    QCOMPARE(ids.first(), dos);
    QCOMPARE(ids.size(), placeIds().size());

    QVERIFY(!controller.canMovePlace(dos, -1));
    QVERIFY(controller.canMovePlace(dos, 1));
    QVERIFY(controller.movePlaceBy(dos, 1));
    QCOMPARE(placeIds().at(1), dos);

    QVERIFY(config.save());
    Config reader;
    QVERIFY(reader.load());
    ApplicationController restored(&reader);
    QStringList restoredIds;
    for (const QVariant &place : restored.places()) {
        const QVariantMap map = place.toMap();
        if (map.value(QStringLiteral("section")).toString() == QLatin1String("places"))
            restoredIds.append(map.value(QStringLiteral("id")).toString());
    }
    QCOMPARE(restoredIds, placeIds());

    controller.removePlace(QUrl::fromLocalFile(uno).toString());
    controller.removePlace(QUrl::fromLocalFile(dos).toString());
    QVERIFY(!placeIds().contains(uno));
}

void TestController::previewAndOpenWithResolveRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("nota.txt")), QByteArrayLiteral("contenido\n"));
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("carpeta")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    const int fileRow = controller.rowForName(QStringLiteral("nota.txt"));
    const int dirRow = controller.rowForName(QStringLiteral("carpeta"));
    QVERIFY(fileRow >= 0);
    QVERIFY(dirRow >= 0);

    const QVariantMap file = controller.previewRow(fileRow);
    QCOMPARE(file.value(QStringLiteral("kind")).toString(), QStringLiteral("text"));
    QCOMPARE(file.value(QStringLiteral("text")).toString(), QStringLiteral("contenido\n"));

    QCOMPARE(controller.previewRow(dirRow).value(QStringLiteral("kind")).toString(),
             QStringLiteral("directory"));
    QVERIFY(controller.previewRow(999).isEmpty());

    QVERIFY(controller.openWithApps(dirRow).isEmpty());
    QVERIFY(controller.openWithApps(999).isEmpty());
    controller.openWithApps(fileRow);
}

void TestController::actionsTriggerControllerState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));
    createFile(dir.filePath(QStringLiteral("b.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    ActionRegistry actions(&controller);
    startInDirectory(controller, dir.path());

    actions.trigger(QStringLiteral("select.all"));
    QCOMPARE(controller.selectionCount(), 2);

    actions.trigger(QStringLiteral("select.none"));
    QCOMPARE(controller.selectionCount(), 0);

    actions.trigger(QStringLiteral("view.list"));
    QCOMPARE(controller.viewMode(), QStringLiteral("list"));

    actions.trigger(QStringLiteral("theme.dark"));
    QCOMPARE(controller.themeMode(), QStringLiteral("dark"));

    actions.trigger(QStringLiteral("icons.large"));
    QCOMPARE(controller.iconSize(), QStringLiteral("large"));
    QCOMPARE(actionById(actions.mainMenu(), QStringLiteral("icons.large"))
                 .value(QStringLiteral("checked")).toBool(), true);
    QCOMPARE(actionById(actions.mainMenu(), QStringLiteral("icons.normal"))
                 .value(QStringLiteral("checked")).toBool(), false);

    actions.trigger(QStringLiteral("accion.inexistente"));
    QCOMPARE(controller.themeMode(), QStringLiteral("dark"));
}

void TestController::actionsRequestUi()
{
    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    ActionRegistry actions(&controller);

    QSignalSpy spy(&actions, &ActionRegistry::uiRequested);

    actions.trigger(QStringLiteral("view.settings"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.constFirst().constFirst().toString(), QStringLiteral("view.settings"));

    actions.trigger(QStringLiteral("nav.path"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.constLast().constFirst().toString(), QStringLiteral("nav.path"));

    actions.trigger(QStringLiteral("theme.light"));
    QCOMPARE(spy.count(), 2);
}

void TestController::actionsTrackEnabledState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    ActionRegistry actions(&controller);
    startInDirectory(controller, dir.path());

    QVERIFY(actionById(actions.commands(), QStringLiteral("select.all"))
                .value(QStringLiteral("enabled")).toBool());
    QVERIFY(!actionById(actions.commands(), QStringLiteral("select.none"))
                 .value(QStringLiteral("enabled")).toBool());

    QSignalSpy spy(&actions, &ActionRegistry::entriesChanged);
    controller.selectAll();

    QVERIFY(spy.count() >= 1);
    QVERIFY(actionById(actions.commands(), QStringLiteral("select.none"))
                .value(QStringLiteral("enabled")).toBool());
}

void TestController::selectRowsReplacesAddsAndToggles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));
    createFile(dir.filePath(QStringLiteral("b.txt")));
    createFile(dir.filePath(QStringLiteral("c.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    const int a = controller.rowForName(QStringLiteral("a.txt"));
    const int b = controller.rowForName(QStringLiteral("b.txt"));
    const int c = controller.rowForName(QStringLiteral("c.txt"));
    QVERIFY(a >= 0);
    QVERIFY(b >= 0);
    QVERIFY(c >= 0);

    QSignalSpy spy(&controller, &ApplicationController::selectionChanged);

    controller.selectRows({a, c}, 0);
    QCOMPARE(controller.selectionCount(), 2);
    QVERIFY(controller.isSelected(a));
    QVERIFY(controller.isSelected(c));
    QCOMPARE(spy.count(), 1);

    controller.selectRows({a, c}, 0);
    QCOMPARE(spy.count(), 1);

    controller.selectRows({b, 99, -1}, 1);
    QCOMPARE(controller.selectionCount(), 3);
    QVERIFY(controller.isSelected(b));
    QCOMPARE(spy.count(), 2);

    controller.selectRows({a}, 2);
    QVERIFY(!controller.isSelected(a));
    QCOMPARE(controller.selectionCount(), 2);
    QCOMPARE(spy.count(), 3);

    controller.selectRows({a}, 2);
    QVERIFY(controller.isSelected(a));
    QCOMPARE(controller.selectionCount(), 3);
    QCOMPARE(spy.count(), 4);

    controller.selectRows({a, a, -1, 99}, 0);
    QCOMPARE(controller.selectionCount(), 1);
    QCOMPARE(spy.count(), 5);

    controller.selectRows({}, 0);
    QCOMPARE(controller.selectionCount(), 0);
    QCOMPARE(spy.count(), 6);
}

void TestController::selectedRowsFollowsOrder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));
    createFile(dir.filePath(QStringLiteral("b.txt")));
    createFile(dir.filePath(QStringLiteral("c.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    const int a = controller.rowForName(QStringLiteral("a.txt"));
    const int c = controller.rowForName(QStringLiteral("c.txt"));
    QVERIFY(a >= 0);
    QVERIFY(c >= 0);

    controller.selectRows({c, a}, 0);
    const QVector<int> rows = controller.selectedRows();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0), a);
    QCOMPARE(rows.at(1), c);

    controller.clearSelection();
    QVERIFY(controller.selectedRows().isEmpty());
}

void TestController::cutClipboardSurvivesUnrelatedOperations()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString targetDir = dir.filePath(QStringLiteral("dest"));
    QVERIFY(QDir().mkpath(targetDir));
    createFile(dir.filePath(QStringLiteral("a.txt")));
    const QString other = dir.filePath(QStringLiteral("b.txt"));
    createFile(other);

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    controller.selectRows({controller.rowForName(QStringLiteral("a.txt"))}, 0);
    controller.cutSelection();
    QVERIFY(controller.hasClipboard());
    QVERIFY(controller.clipboardCut());

    controller.dropUrls({QUrl::fromLocalFile(other)}, Qt::MoveAction,
                        QUrl::fromLocalFile(targetDir).toString());
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(targetDir + QStringLiteral("/b.txt")), 5000);

    QVERIFY(controller.hasClipboard());
    QVERIFY(controller.clipboardCut());
}

void TestController::clipboardMarkReflectsCopyAndCut()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    createFile(dir.filePath(QStringLiteral("a.txt")));
    createFile(dir.filePath(QStringLiteral("b.txt")));

    Config config;
    QVERIFY(config.load());
    ApplicationController controller(&config);
    startInDirectory(controller, dir.path());

    const QString a = QUrl::fromLocalFile(dir.filePath(QStringLiteral("a.txt"))).toString();
    const QString b = QUrl::fromLocalFile(dir.filePath(QStringLiteral("b.txt"))).toString();
    QCOMPARE(controller.clipboardMark(a), 0);

    controller.selectRows({controller.rowForName(QStringLiteral("a.txt"))}, 0);
    controller.copySelection();
    QCOMPARE(controller.clipboardMark(a), 1);
    QCOMPARE(controller.clipboardMark(b), 0);

    controller.cutSelection();
    QCOMPARE(controller.clipboardMark(a), 2);
    QCOMPARE(controller.clipboardMark(b), 0);
}

QTEST_MAIN(TestController)
#include "tst_controller.moc"
