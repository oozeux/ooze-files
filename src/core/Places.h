#pragma once

#include <QStringList>
#include <QVariantList>

namespace Places {
QVariantList build(const QVariantList &custom = {}, const QStringList &order = {});
QStringList orderedIds(const QVariantList &places);
}
