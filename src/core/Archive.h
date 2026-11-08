#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace Archive {

struct Command
{
    QString program;
    QStringList arguments;
    QString workingDirectory;

    [[nodiscard]] bool isValid() const { return !program.isEmpty(); }
};

[[nodiscard]] bool isArchiveName(const QString &fileName);
[[nodiscard]] QString baseNameWithoutSuffix(const QString &fileName);
[[nodiscard]] QString suffixForFormat(const QString &formatId);
[[nodiscard]] QVariantList availableFormats();

[[nodiscard]] Command compressCommand(const QString &formatId, const QString &archivePath,
                                      const QStringList &names, const QString &workingDirectory,
                                      QString *error);
[[nodiscard]] Command extractCommand(const QString &archivePath, const QString &destination,
                                     QString *error);

}
