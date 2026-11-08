#pragma once

#include <QList>
#include <QString>
#include <QUrl>
#include <QVariantList>

namespace OpenWith {

QVariantList appsForMime(const QString &mime);

QVariantList otherApps(const QString &mime);

QVariantList defaultAppStates();

bool setDefault(const QString &categoryId, const QString &appId, QString *error);

bool launch(const QString &appId, const QString &mime, const QList<QUrl> &urls, QString *error);

}
