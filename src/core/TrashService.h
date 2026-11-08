#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUrl>
#include <QVector>

struct TrashDir
{
    QByteArray root;
    QByteArray topdir;

    [[nodiscard]] bool isValid() const { return !root.isEmpty(); }
    bool operator==(const TrashDir &other) const { return root == other.root; }
};

struct TrashItem
{
    TrashDir dir;
    QByteArray name;
    QByteArray path;
    QByteArray infoPath;
    QByteArray originalPath;
    QDateTime deleted;
    bool hasInfo = false;

    [[nodiscard]] QByteArray displayBytes() const;
};

namespace TrashService {
struct Location
{
    bool root = false;
    TrashDir dir;
    QByteArray relative;
    QByteArray realPath;
};

QByteArray homeTrashRoot();
QVector<TrashDir> discoverDirs();
QVector<TrashItem> items(const TrashDir &dir);
QVector<TrashItem> allItems();
bool hasItems();

bool locate(const QByteArray &path, TrashItem *item);

QUrl rootUrl();
QUrl virtualUrl(const TrashDir &dir, const QByteArray &relative);
QUrl virtualUrlForPath(const QByteArray &path);
bool resolve(const QUrl &url, Location *location);

QVector<QByteArray> watchPaths();

bool forget(const TrashItem &item);
bool removeItem(const TrashItem &item, QString *error);
int purgeOrphans(const TrashDir &dir);
}
