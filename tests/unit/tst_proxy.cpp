#include "core/FileListModel.h"
#include "core/FileListProxyModel.h"

#include <QSet>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

namespace {
VfsEntry makeEntry(const QString &name, EntryKind kind, qint64 size = 0, bool hidden = false)
{
    VfsEntry entry;
    entry.id = name.toUtf8();
    entry.displayName = name;
    entry.url = QUrl::fromLocalFile(QStringLiteral("/tmp/") + name);
    entry.path = (QStringLiteral("/tmp/") + name).toUtf8();
    entry.kind = kind;
    entry.size = size;
    entry.iconName = kind == EntryKind::Directory ? QStringLiteral("folder")
                                                 : QStringLiteral("text-x-generic");
    if (hidden)
        entry.flags |= FlagHidden;
    return entry;
}

void populate(FileListModel &model)
{
    model.appendEntries({makeEntry(QStringLiteral("zeta.txt"), EntryKind::File, 300),
                         makeEntry(QStringLiteral("alpha"), EntryKind::File, 100),
                         makeEntry(QStringLiteral("Beta"), EntryKind::Directory, -1),
                         makeEntry(QStringLiteral(".oculto"), EntryKind::File, 5, true),
                         makeEntry(QStringLiteral("carpeta"), EntryKind::Directory, -1)});
}
}

class TestProxy : public QObject
{
    Q_OBJECT

private slots:
    void hidesHiddenEntriesByDefault();
    void filtersByName();
    void sortsFoldersFirst();
    void sortsDescendingKeepingFoldersFirst();
    void sortsBySize();
    void syncsIncrementalChanges();
};

void TestProxy::hidesHiddenEntriesByDefault()
{
    FileListModel model;
    FileListProxyModel proxy;
    proxy.setSourceModel(&model);
    populate(model);

    QCOMPARE(proxy.rowCount(), 4);

    proxy.setShowHidden(true);
    QCOMPARE(proxy.rowCount(), 5);
}

void TestProxy::filtersByName()
{
    FileListModel model;
    FileListProxyModel proxy;
    proxy.setSourceModel(&model);
    populate(model);

    proxy.setFilterText(QStringLiteral("car"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(FileListModel::NameRole).toString(),
             QStringLiteral("carpeta"));

    proxy.setFilterText(QStringLiteral("nothing-here"));
    QCOMPARE(proxy.rowCount(), 0);
}

void TestProxy::sortsFoldersFirst()
{
    FileListModel model;
    FileListProxyModel proxy;
    proxy.setSourceModel(&model);
    populate(model);

    QCOMPARE(proxy.index(0, 0).data(FileListModel::IsDirRole).toBool(), true);
    QCOMPARE(proxy.index(1, 0).data(FileListModel::IsDirRole).toBool(), true);
    QCOMPARE(proxy.index(0, 0).data(FileListModel::NameRole).toString(),
             QStringLiteral("Beta"));
    QCOMPARE(proxy.index(1, 0).data(FileListModel::NameRole).toString(),
             QStringLiteral("carpeta"));
    QCOMPARE(proxy.index(2, 0).data(FileListModel::NameRole).toString(),
             QStringLiteral("alpha"));
}

void TestProxy::sortsDescendingKeepingFoldersFirst()
{
    FileListModel model;
    FileListProxyModel proxy;
    proxy.setSourceModel(&model);
    populate(model);
    proxy.setDescending(true);

    QCOMPARE(proxy.index(0, 0).data(FileListModel::IsDirRole).toBool(), true);
    QCOMPARE(proxy.index(1, 0).data(FileListModel::IsDirRole).toBool(), true);
    QCOMPARE(proxy.index(2, 0).data(FileListModel::NameRole).toString(),
             QStringLiteral("zeta.txt"));
    QCOMPARE(proxy.index(3, 0).data(FileListModel::NameRole).toString(),
             QStringLiteral("alpha"));
}

void TestProxy::sortsBySize()
{
    FileListModel model;
    FileListProxyModel proxy;
    proxy.setSourceModel(&model);
    populate(model);
    proxy.setSortMode(FileListProxyModel::SortBySize);

    QCOMPARE(proxy.index(2, 0).data(FileListModel::NameRole).toString(),
             QStringLiteral("alpha"));
    QCOMPARE(proxy.index(3, 0).data(FileListModel::NameRole).toString(),
             QStringLiteral("zeta.txt"));
}

void TestProxy::syncsIncrementalChanges()
{
    FileListModel model;
    FileListProxyModel proxy;
    proxy.setSourceModel(&model);
    populate(model);

    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

    model.syncEntries({makeEntry(QStringLiteral("alpha"), EntryKind::File, 100),
                       makeEntry(QStringLiteral("Beta"), EntryKind::Directory, -1),
                       makeEntry(QStringLiteral("carpeta"), EntryKind::Directory, -1),
                       makeEntry(QStringLiteral("gamma.txt"), EntryKind::File, 200)});

    QCOMPARE(model.rowCount(), 4);
    QVERIFY(removed.count() >= 1);
    QCOMPARE(inserted.count(), 1);

    QSet<QString> names;
    for (int row = 0; row < model.rowCount(); ++row)
        names.insert(model.index(row, 0).data(FileListModel::NameRole).toString());

    QVERIFY(names.contains(QStringLiteral("gamma.txt")));
    QVERIFY(!names.contains(QStringLiteral("zeta.txt")));
    QVERIFY(!names.contains(QStringLiteral(".oculto")));
    QCOMPARE(proxy.rowCount(), 4);
}

QTEST_MAIN(TestProxy)
#include "tst_proxy.moc"
