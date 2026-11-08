#include "core/TerminalLauncher.h"

#include "core/PathUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <QLoggingCategory>
#include <QCoreApplication>

Q_LOGGING_CATEGORY(lcTerminal, "ooze.files.terminal")

namespace {
const QStringList kCandidates = {
    QStringLiteral("foot"),
    QStringLiteral("kitty"),
    QStringLiteral("ghostty"),
    QStringLiteral("alacritty"),
    QStringLiteral("wezterm"),
    QStringLiteral("konsole"),
    QStringLiteral("kgx"),
    QStringLiteral("gnome-terminal"),
    QStringLiteral("cosmic-term"),
    QStringLiteral("xterm"),
};

QStringList adapterArgs(const QString &program, const QString &directory)
{
    const QString exe = QFileInfo(program).fileName();

    if (exe == QStringLiteral("foot") || exe == QStringLiteral("footclient"))
        return {QStringLiteral("--working-directory=") + directory};
    if (exe == QStringLiteral("kitty"))
        return {QStringLiteral("--directory"), directory};
    if (exe == QStringLiteral("ghostty"))
        return {QStringLiteral("--working-directory=") + directory};
    if (exe == QStringLiteral("alacritty"))
        return {QStringLiteral("--working-directory"), directory};
    if (exe == QStringLiteral("wezterm"))
        return {QStringLiteral("start"), QStringLiteral("--cwd"), directory};
    if (exe == QStringLiteral("konsole"))
        return {QStringLiteral("--workdir"), directory};
    if (exe == QStringLiteral("kgx") || exe == QStringLiteral("gnome-terminal")
        || exe == QStringLiteral("cosmic-term"))
        return {QStringLiteral("--working-directory=") + directory};

    return {};
}
}

TerminalLauncher::TerminalLauncher(QObject *parent)
    : QObject(parent)
{
}

QString TerminalLauncher::detect(const QString &configuredTerminal)
{
    if (!configuredTerminal.trimmed().isEmpty()) {
        const QString found = QStandardPaths::findExecutable(configuredTerminal.trimmed());
        if (!found.isEmpty())
            return found;
    }

    const QString fromEnv = qEnvironmentVariable("TERMINAL");
    if (!fromEnv.isEmpty()) {
        const QString found = QStandardPaths::findExecutable(fromEnv);
        if (!found.isEmpty())
            return found;
    }

    for (const QString &candidate : kCandidates) {
        const QString found = QStandardPaths::findExecutable(candidate);
        if (!found.isEmpty())
            return found;
    }

    return {};
}

bool TerminalLauncher::openAt(const QUrl &target, const QString &configuredTerminal)
{
    QString directory;
    if (target.isLocalFile()) {
        const QString path = target.toLocalFile();
        QFileInfo info(path);
        directory = info.isDir() ? path : info.absolutePath();
    } else {
        m_lastError = QCoreApplication::translate("Ooze", "Solo se puede abrir un terminal en una carpeta local");
        emit lastErrorChanged();
        return false;
    }

    if (directory.isEmpty() || !QFileInfo(directory).isDir()) {
        m_lastError = QCoreApplication::translate("Ooze", "La carpeta no existe: %1").arg(directory);
        emit lastErrorChanged();
        return false;
    }

    const QString program = detect(configuredTerminal);
    if (program.isEmpty()) {
        m_lastError = QStringLiteral(
            "No se encontró ningún emulador de terminal. "
            "Define \"behavior.terminal\" en config.json o la variable TERMINAL.");
        emit lastErrorChanged();
        return false;
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments(adapterArgs(program, directory));
    process.setWorkingDirectory(directory);

    if (!process.startDetached()) {
        m_lastError = QCoreApplication::translate("Ooze", "No se pudo ejecutar %1: %2")
                          .arg(program, process.errorString());
        qCWarning(lcTerminal) << m_lastError;
        emit lastErrorChanged();
        return false;
    }

    qCInfo(lcTerminal) << "Terminal abierto en" << directory << "con" << program;
    if (!m_lastError.isEmpty()) {
        m_lastError.clear();
        emit lastErrorChanged();
    }
    return true;
}
