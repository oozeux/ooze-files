#include "core/CancelToken.h"
#include "core/RecentFiles.h"
#include "vfs/RecentVfs.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

namespace {
const char kHeader[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<xbel version=\"1.0\" xmlns:bookmark=\"http://www.freedesktop.org/standards/desktop-bookmarks\""
    " xmlns:mime=\"http://www.freedesktop.org/standards/shared-mime-info\">\n";

QByteArray bookmark(const QString &path, const QString &modified, const QString &visited,
                    const QString &mime = QStringLiteral("text/plain"),
                    const QString &group = QString(), const QString &application = QString())
{
    const QString href = QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
    QString xml = QStringLiteral("<bookmark href=\"%1\" added=\"%2\" modified=\"%2\" visited=\"%3\">"
                                 "<info><metadata owner=\"http://freedesktop.org\">"
                                 "<mime:mime-type type=\"%4\"/>")
                      .arg(href, modified, visited, mime);
    if (!group.isEmpty()) {
        xml += QStringLiteral("<bookmark:groups><bookmark:group>%1</bookmark:group></bookmark:groups>").arg(group);
    }
    if (!application.isEmpty()) {
        xml += QStringLiteral("<bookmark:applications><bookmark:application name=\"ed\" exec=\"ed %u\""
                              " modified=\"%1\" count=\"1\"/></bookmark:applications>")
                   .arg(application);
    }
    xml += QStringLiteral("</metadata></info></bookmark>\n");
    return xml.toUtf8();
}

void writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(content);
}
}

class TestRecentFiles : public QObject
{
    Q_OBJECT

private slots:
    void parsesPathsDatesAndMime();
    void ignoresPrivateForeignAndInvalidEntries();
    void deduplicatesKeepingNewestActivity();
    void toleratesBrokenXml();
    void backendListsExistingFilesNewestFirst();
    void backendRejectsOtherLocations();
};

void TestRecentFiles::parsesPathsDatesAndMime()
{
    QByteArray xml(kHeader);
    xml += bookmark(QStringLiteral("/tmp/ooze recents/a b.txt"), QStringLiteral("2026-01-02T10:00:00Z"),
                    QStringLiteral("2026-01-03T10:00:00Z"), QStringLiteral("text/plain"), {},
                    QStringLiteral("2026-01-05T10:00:00Z"));
    xml += "</xbel>\n";

    const QVector<RecentItem> items = RecentFiles::parse(xml);
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.first().path, QByteArray("/tmp/ooze recents/a b.txt"));
    QCOMPARE(items.first().mimeType, QStringLiteral("text/plain"));
    QCOMPARE(items.first().activity, QDateTime::fromString(QStringLiteral("2026-01-05T10:00:00Z"), Qt::ISODate));
}

void TestRecentFiles::ignoresPrivateForeignAndInvalidEntries()
{
    QByteArray xml(kHeader);
    xml += bookmark(QStringLiteral("/tmp/ok.txt"), QStringLiteral("2026-01-01T00:00:00Z"),
                    QStringLiteral("2026-01-01T00:00:00Z"));
    xml += bookmark(QStringLiteral("/tmp/secreto.txt"), QStringLiteral("2026-01-02T00:00:00Z"),
                    QStringLiteral("2026-01-02T00:00:00Z"), QStringLiteral("text/plain"), QStringLiteral("private"));
    xml += "<bookmark href=\"https://example.invalid/x\" added=\"2026-01-03T00:00:00Z\"/>\n";
    xml += "<bookmark href=\"file://otro-host/tmp/x\" added=\"2026-01-03T00:00:00Z\"/>\n";
    xml += "<bookmark added=\"2026-01-03T00:00:00Z\"/>\n";
    xml += "</xbel>\n";

    const QVector<RecentItem> items = RecentFiles::parse(xml);
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.first().path, QByteArray("/tmp/ok.txt"));
}

void TestRecentFiles::deduplicatesKeepingNewestActivity()
{
    QByteArray xml(kHeader);
    xml += bookmark(QStringLiteral("/tmp/a.txt"), QStringLiteral("2026-01-01T00:00:00Z"),
                    QStringLiteral("2026-01-01T00:00:00Z"));
    xml += bookmark(QStringLiteral("/tmp/b.txt"), QStringLiteral("2026-03-01T00:00:00Z"),
                    QStringLiteral("2026-03-01T00:00:00Z"));
    xml += bookmark(QStringLiteral("/tmp/a.txt"), QStringLiteral("2026-05-01T00:00:00Z"),
                    QStringLiteral("2026-05-01T00:00:00Z"));
    xml += "</xbel>\n";

    const QVector<RecentItem> items = RecentFiles::parse(xml);
    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).path, QByteArray("/tmp/a.txt"));
    QCOMPARE(items.at(1).path, QByteArray("/tmp/b.txt"));
}

void TestRecentFiles::toleratesBrokenXml()
{
    QByteArray xml(kHeader);
    xml += bookmark(QStringLiteral("/tmp/ok.txt"), QStringLiteral("2026-01-01T00:00:00Z"),
                    QStringLiteral("2026-01-01T00:00:00Z"));
    xml += "<bookmark href=\"file:///tmp/roto.txt\" added=\"2026-01-0";

    const QVector<RecentItem> items = RecentFiles::parse(xml);
    QVERIFY(items.size() >= 1);
    QCOMPARE(items.first().path, QByteArray("/tmp/ok.txt"));

    QVERIFY(RecentFiles::parse(QByteArray()).isEmpty());
    QVERIFY(RecentFiles::parse(QByteArray("no es xml")).isEmpty());
    QVERIFY(RecentFiles::load(QStringLiteral("/no/existe/recently-used.xbel")).isEmpty());
}

void TestRecentFiles::backendListsExistingFilesNewestFirst()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString older = temp.filePath(QStringLiteral("antiguo.txt"));
    const QString newer = temp.filePath(QStringLiteral("nuevo.txt"));
    const QString missing = temp.filePath(QStringLiteral("borrado.txt"));
    writeFile(older, "a");
    writeFile(newer, "bb");

    QByteArray xml(kHeader);
    xml += bookmark(older, QStringLiteral("2026-01-01T00:00:00Z"), QStringLiteral("2026-01-01T00:00:00Z"));
    xml += bookmark(missing, QStringLiteral("2026-06-01T00:00:00Z"), QStringLiteral("2026-06-01T00:00:00Z"));
    xml += bookmark(newer, QStringLiteral("2026-03-01T00:00:00Z"), QStringLiteral("2026-03-01T00:00:00Z"));
    xml += "</xbel>\n";
    const QString xbel = temp.filePath(QStringLiteral("recently-used.xbel"));
    writeFile(xbel, xml);

    RecentVfs backend(xbel);
    QVector<VfsEntry> entries;
    VfsError finalError;
    bool finished = false;
    CancelToken token;
    backend.list(RecentVfs::rootUrl(),
                 [&](QVector<VfsEntry> &&batch, const VfsError &error) {
                     if (batch.isEmpty()) {
                         finished = true;
                         finalError = error;
                         return;
                     }
                     entries += batch;
                 },
                 token);

    QVERIFY(finished);
    QVERIFY(!finalError.isValid());
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).displayName, QStringLiteral("nuevo.txt"));
    QCOMPARE(entries.at(1).displayName, QStringLiteral("antiguo.txt"));
    QCOMPARE(entries.at(0).path, newer.toUtf8());
    QCOMPARE(entries.at(0).id, newer.toUtf8());
    QCOMPARE(entries.at(0).size, qint64(2));
    QVERIFY(entries.at(0).location.endsWith(QFileInfo(temp.path()).fileName()));
    QVERIFY(entries.at(0).activity.isValid());
    QVERIFY(hasFlag(entries.at(0), FlagReadable));
}

void TestRecentFiles::backendRejectsOtherLocations()
{
    RecentVfs backend(QStringLiteral("/no/existe.xbel"));
    CancelToken token;
    VfsError error;
    backend.list(QUrl(QStringLiteral("recent:///otra")),
                 [&](QVector<VfsEntry> &&, const VfsError &e) { error = e; }, token);
    QCOMPARE(error.code, VfsError::NotFound);

    QVERIFY(backend.exists(RecentVfs::rootUrl()));
    QVERIFY(!backend.exists(QUrl(QStringLiteral("recent:///otra"))));
    QVERIFY(!backend.capabilities(RecentVfs::rootUrl()).canWrite);
}

QTEST_MAIN(TestRecentFiles)
#include "tst_recentfiles.moc"
