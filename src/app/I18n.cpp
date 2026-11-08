#include "app/I18n.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

namespace {
constexpr auto kSystem = "system";
constexpr auto kSpanish = "es";
constexpr auto kEnglish = "en";

QTranslator *translator()
{
    static QTranslator instance;
    return &instance;
}

int &generationCounter()
{
    static int value = 0;
    return value;
}
}

namespace I18n {

QStringList supportedSettings()
{
    return {QLatin1String(kSystem), QLatin1String(kSpanish), QLatin1String(kEnglish)};
}

QString resolve(const QString &setting)
{
    if (setting == QLatin1String(kSpanish) || setting == QLatin1String(kEnglish))
        return setting;

    const QString system = QLocale::system().name();
    return system.startsWith(QLatin1String("es")) ? QLatin1String(kSpanish)
                                                  : QLatin1String(kEnglish);
}

void apply(const QString &setting)
{
    const QString language = resolve(setting);

    QCoreApplication::removeTranslator(translator());

    if (language == QLatin1String(kEnglish)) {
        if (translator()->load(QStringLiteral(":/i18n/ooze-files_en.qm")))
            QCoreApplication::installTranslator(translator());
    }

    QLocale::setDefault(setting == QLatin1String(kSystem) ? QLocale::system() : QLocale(language));
    ++generationCounter();
}

int generation()
{
    return generationCounter();
}

}
