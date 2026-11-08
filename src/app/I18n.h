#pragma once

#include <QString>
#include <QStringList>

namespace I18n {

[[nodiscard]] QStringList supportedSettings();
[[nodiscard]] QString resolve(const QString &setting);
void apply(const QString &setting);
[[nodiscard]] int generation();

}
