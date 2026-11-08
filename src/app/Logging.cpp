#include "app/Logging.h"

#include <QLoggingCategory>

namespace Logging {
void install(bool verbose)
{
    qSetMessagePattern(QStringLiteral("%{time hh:mm:ss.zzz} [%{category}] %{message}"));

    if (verbose) {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "ooze.files.*.debug=true\n"
            "ooze.files.*.info=true"));
    } else {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "ooze.files.*.debug=false"));
    }
}
}
