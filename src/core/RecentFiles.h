#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

struct RecentItem
{
    QByteArray path;
    QDateTime activity;
    QString mimeType;
};

namespace RecentFiles {
constexpr qint64 kMaxFileBytes = 32 * 1024 * 1024;

QString defaultFilePath();
QVector<RecentItem> parse(const QByteArray &xml);
QVector<RecentItem> load(const QString &filePath);
}
