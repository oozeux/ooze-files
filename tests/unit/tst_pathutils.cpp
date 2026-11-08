#include "core/PathUtils.h"

#include <QTest>
#include <QUrl>
#include <QDir>

class TestPathUtils : public QObject
{
    Q_OBJECT

private slots:
    void recentLocationHasNoParent();
    void bytesRoundTripKeepsInvalidUtf8();
    void normalizedCollapsesDotSegments();
    void parentIsLexical();
    void ancestorComparisonIsComponentWise();
    void childKeepsBytes();
    void breadcrumbStartsAtHome();
    void formatSizeHandlesUnknown();
    void formatDateHandlesInvalid();
};

void TestPathUtils::bytesRoundTripKeepsInvalidUtf8()
{
    const QByteArray name("caf\xe9.txt");
    const QUrl url = PathUtils::child(QUrl::fromLocalFile(QStringLiteral("/tmp")), name);

    QCOMPARE(PathUtils::toLocalBytes(url), QByteArray("/tmp/caf\xe9.txt"));
    QVERIFY(!PathUtils::displayName(name).isEmpty());
}

void TestPathUtils::normalizedCollapsesDotSegments()
{
    const QUrl url(QStringLiteral("file:///a//b/./c/../d"));
    QCOMPARE(PathUtils::toLocalBytes(PathUtils::normalized(url)), QByteArray("/a/b/d"));

    const QUrl root = PathUtils::normalized(QUrl::fromLocalFile(QStringLiteral("/")));
    QCOMPARE(PathUtils::toLocalBytes(root), QByteArray("/"));
}

void TestPathUtils::parentIsLexical()
{
    QCOMPARE(PathUtils::parent(QUrl::fromLocalFile(QStringLiteral("/a/b/c"))),
             QUrl::fromLocalFile(QStringLiteral("/a/b")));
    QCOMPARE(PathUtils::parent(QUrl::fromLocalFile(QStringLiteral("/a"))),
             QUrl::fromLocalFile(QStringLiteral("/")));
    QCOMPARE(PathUtils::parent(QUrl::fromLocalFile(QStringLiteral("/"))),
             QUrl::fromLocalFile(QStringLiteral("/")));
}

void TestPathUtils::ancestorComparisonIsComponentWise()
{
    const QUrl base = QUrl::fromLocalFile(QStringLiteral("/home/ana"));
    QVERIFY(PathUtils::isSameOrAncestor(base, QUrl::fromLocalFile(QStringLiteral("/home/ana"))));
    QVERIFY(PathUtils::isSameOrAncestor(base, QUrl::fromLocalFile(QStringLiteral("/home/ana/docs"))));
    QVERIFY(!PathUtils::isSameOrAncestor(base, QUrl::fromLocalFile(QStringLiteral("/home/ana2"))));
    QVERIFY(PathUtils::isSameOrAncestor(QUrl::fromLocalFile(QStringLiteral("/")),
                                        QUrl::fromLocalFile(QStringLiteral("/etc"))));
}

void TestPathUtils::childKeepsBytes()
{
    const QByteArray name("with space \xff");
    const QUrl dir = QUrl::fromLocalFile(QStringLiteral("/tmp/dir"));
    const QUrl child = PathUtils::child(dir, name);

    QVERIFY(child.isLocalFile());
    QCOMPARE(PathUtils::toLocalBytes(child), QByteArray("/tmp/dir/with space \xff"));
}

void TestPathUtils::breadcrumbStartsAtHome()
{
    const QUrl home = QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/docs/x"));
    const QVariantList crumbs = PathUtils::breadcrumb(home);

    QVERIFY(crumbs.size() >= 3);
    QCOMPARE(crumbs.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Inicio"));
    QCOMPARE(crumbs.last().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("x"));
}

void TestPathUtils::formatSizeHandlesUnknown()
{
    QCOMPARE(PathUtils::formatSize(-1), QStringLiteral("—"));
    QVERIFY(!PathUtils::formatSize(1024).isEmpty());
}

void TestPathUtils::formatDateHandlesInvalid()
{
    QCOMPARE(PathUtils::formatDate({}), QStringLiteral("—"));
}

void TestPathUtils::recentLocationHasNoParent()
{
    const QUrl recent(QStringLiteral("recent:///"));
    QCOMPARE(PathUtils::parent(recent), recent);
    QCOMPARE(PathUtils::normalized(recent), recent);

    const QVariantList crumbs = PathUtils::breadcrumb(recent);
    QCOMPARE(crumbs.size(), 1);
    QCOMPARE(crumbs.first().toMap().value(QStringLiteral("url")).toString(), recent.toString());
}

QTEST_MAIN(TestPathUtils)
#include "tst_pathutils.moc"
