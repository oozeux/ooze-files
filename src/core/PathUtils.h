#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUrl>
#include <QVariantList>

namespace PathUtils {
QByteArray toLocalBytes(const QUrl &url);

QUrl fromLocalBytes(const QByteArray &bytes);

QString displayName(const QByteArray &bytes);

QUrl normalized(const QUrl &url);

QUrl parent(const QUrl &url);
bool isVirtual(const QUrl &url);
QString homeAbbreviated(const QByteArray &folder);

QByteArray childBytes(const QByteArray &dir, const QByteArray &name);

QUrl child(const QUrl &dir, const QByteArray &name);

bool isSameOrAncestor(const QUrl &ancestor, const QUrl &url);

QVariantList breadcrumb(const QUrl &url);

QString formatSize(qint64 bytes);

QString formatDate(const QDateTime &dt);
}
