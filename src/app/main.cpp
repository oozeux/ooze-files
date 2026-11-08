#include "app/ApplicationController.h"
#include "app/ActionRegistry.h"
#include "app/DragPreviewProvider.h"
#include "app/IconProvider.h"
#include "app/I18n.h"
#include "app/IconTheme.h"
#include "app/Logging.h"
#include "app/PdfPreviewProvider.h"
#include "app/SearchModel.h"
#include "app/SingleInstance.h"
#include "app/ThumbnailProvider.h"
#include "app/Tools.h"
#include "settings/Config.h"
#include "theme/ThemeStore.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMain, "ooze.files.main")

namespace {
QUrl resolveStartupUrl(const QString &argument, QString *selectName)
{
    if (argument.isEmpty())
        return {};

    QString path = argument;
    if (path.startsWith(QStringLiteral("~/")))
        path = QDir::homePath() + path.mid(1);

    const QUrl url = QUrl::fromUserInput(path, QDir::currentPath(),
                                         QUrl::AssumeLocalFile);

    if (!url.isLocalFile())
        return url;

    const QFileInfo info(url.toLocalFile());
    if (!info.exists())
        return {};

    if (info.isDir())
        return QUrl::fromLocalFile(info.absoluteFilePath());

    if (selectName)
        *selectName = info.fileName();
    return QUrl::fromLocalFile(info.absolutePath());
}

QUrl fallbackUrl(const QString &lastPath)
{
    if (!lastPath.isEmpty()) {
        const QUrl url(lastPath);
        if (url.isValid() && url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir())
            return url;
    }
    return QUrl::fromLocalFile(QDir::homePath());
}

void logQmlWarnings(const QList<QQmlError> &warnings)
{
    for (const QQmlError &error : warnings)
        qCWarning(lcMain).noquote() << error.toString();
}

QString earlyConfigPath()
{
    const QByteArray xdg = qgetenv("XDG_CONFIG_HOME");
    const QString base = xdg.isEmpty() ? QDir::homePath() + QStringLiteral("/.config")
                                       : QString::fromLocal8Bit(xdg);
    return base + QStringLiteral("/ooze-files/config.json");
}

// The render backend has to be chosen before QGuiApplication exists, so the
// setting is read straight from the JSON instead of going through Config.
bool lightweightRequested()
{
    if (qEnvironmentVariableIsSet("OOZE_FILES_LIGHTWEIGHT"))
        return qEnvironmentVariableIntValue("OOZE_FILES_LIGHTWEIGHT") != 0;

    QFile file(earlyConfigPath());
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return false;

    return document.object().value(QStringLiteral("ui")).toObject()
        .value(QStringLiteral("lightweight")).toBool();
}
}

int main(int argc, char *argv[])
{
    if (lightweightRequested() && !qEnvironmentVariableIsSet("QT_QUICK_BACKEND")) {
        // Software rendering avoids loading the GPU driver (70 MB RSS with the
        // NVIDIA stack) at the cost of more CPU while scrolling.
        qputenv("QT_QUICK_BACKEND", "software");
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ooze-files"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Ooze Files"));
    QGuiApplication::setOrganizationName(QStringLiteral("Ooze"));
    QGuiApplication::setApplicationVersion(QStringLiteral(OOZE_FILES_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("ooze-files"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Administrador de archivos minimalista para Wayland"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption newTabOption({QStringLiteral("t"), QStringLiteral("new-tab")},
                                    QStringLiteral("Abrir en una pestaña nueva"));
    QCommandLineOption newWindowOption({QStringLiteral("n"), QStringLiteral("new-window")},
                                       QStringLiteral("Abrir en una ventana independiente"));
    QCommandLineOption selectOption({QStringLiteral("s"), QStringLiteral("select")},
                                    QStringLiteral("Seleccionar un archivo en su carpeta"));
    QCommandLineOption debugOption(QStringLiteral("debug"),
                                   QStringLiteral("Registro detallado"));
    QCommandLineOption smokeOption(QStringLiteral("smoke-test"),
                                   QStringLiteral("Cargar la interfaz y salir (CI)"));

    parser.addOption(newTabOption);
    parser.addOption(newWindowOption);
    parser.addOption(selectOption);
    parser.addOption(debugOption);
    parser.addOption(smokeOption);
    parser.addPositionalArgument(QStringLiteral("path"),
                                 QStringLiteral("Carpeta o archivo a abrir"),
                                 QStringLiteral("[path]"));

    parser.process(app);

    Logging::install(parser.isSet(debugOption));

    Config config;
    config.load();
    if (!config.lastError().isEmpty())
        qCWarning(lcMain) << config.lastError();
    I18n::apply(config.language());

    QString selectName;
    QUrl target = resolveStartupUrl(parser.positionalArguments().value(0), &selectName);
    if (!target.isValid())
        target = fallbackUrl(config.lastPath());

    if (parser.isSet(selectOption)) {
        const QString selected = parser.value(selectOption);
        if (!selected.isEmpty()) {
            const QFileInfo info(selected);
            if (info.isFile()) {
                selectName = info.fileName();
                target = QUrl::fromLocalFile(info.absolutePath());
            }
        }
    }

    SingleInstance instance;
    const bool owner = instance.tryAcquire();
    const bool independent = !owner
        && (parser.isSet(newWindowOption) || !config.singleInstance());

    if (independent) {
        config.setPersistent(false);
    } else if (!owner) {
        const SingleInstance::Mode mode = parser.isSet(newTabOption)
            ? SingleInstance::Mode::NewTab
            : SingleInstance::Mode::Open;

        if (instance.notifyExisting(target, selectName, mode))
            return 0;

        qCWarning(lcMain) << "No se pudo contactar con la instancia existente; "
                             "se continúa sin instancia única";
    }

    ApplicationController controller(&config);

    QObject::connect(&instance, &SingleInstance::openRequested, &controller,
                     [&controller](const QUrl &url, bool select) {
                         controller.handleExternalOpen(url, select);
                     });
    QObject::connect(&instance, &SingleInstance::newTabRequested, &controller,
                     [&controller](const QUrl &url) {
                         controller.openInNewTab(url.toString());
                     });
    QObject::connect(&instance, &SingleInstance::newWindowRequested, &controller,
                     [&controller](const QUrl &url) {
                         controller.handleExternalOpen(url, false);
                     });
    QObject::connect(&controller, &ApplicationController::errorOccurred, &app,
                     [](const QString &message) { qCWarning(lcMain) << message; });

    qmlRegisterSingletonInstance("Ooze.Files", 1, 0, "App", &controller);
    qmlRegisterSingletonInstance("Ooze.Files", 1, 0, "Palette", controller.themeStore());

    ActionRegistry actions(&controller);
    qmlRegisterSingletonInstance("Ooze.Files", 1, 0, "Actions", &actions);

    Tools tools;
    qmlRegisterSingletonInstance("Ooze.Files", 1, 0, "Tools", &tools);

    SearchModel search;
    qmlRegisterSingletonInstance("Ooze.Files", 1, 0, "Search", &search);

    IconTheme::apply(controller.effectiveDark(), controller.iconTheme());

    auto *icons = new IconProvider;
    const auto refreshIcons = [&controller, icons] {
        IconTheme::apply(controller.effectiveDark(), controller.iconTheme());
        icons->clearCache();
    };
    QObject::connect(&controller, &ApplicationController::effectiveDarkChanged, &app,
                     refreshIcons);
    QObject::connect(&controller, &ApplicationController::iconThemeChanged, &app,
                     refreshIcons);

    QQmlApplicationEngine engine;
    QObject::connect(&controller, &ApplicationController::languageChanged, &engine,
                     [&engine] { engine.retranslate(); });
    QObject::connect(&engine, &QQmlEngine::warnings, &app, &logQmlWarnings);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { qCCritical(lcMain) << "No se pudo crear la ventana principal"; },
                     Qt::QueuedConnection);
    engine.addImageProvider(QStringLiteral("icons"), icons);
    engine.addImageProvider(QStringLiteral("dragpreview"),
                            new DragPreviewProvider(icons));
    engine.addImageProvider(QStringLiteral("thumbnails"), new ThumbnailProvider);
    engine.addImageProvider(QStringLiteral("previews"), new PdfPreviewProvider);
    engine.loadFromModule("Ooze.Files", "Main");

    if (engine.rootObjects().isEmpty()) {
        qCCritical(lcMain) << "No se pudo cargar la interfaz";
        return 1;
    }

    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
        const auto present = [window] {
            if (!window->isVisible())
                window->setVisible(true);
            window->requestActivate();
        };

        QObject::connect(&instance, &SingleInstance::openRequested, &app,
                         [present](const QUrl &, bool) { present(); });
        QObject::connect(&instance, &SingleInstance::newTabRequested, &app,
                         [present](const QUrl &) { present(); });
        QObject::connect(&instance, &SingleInstance::newWindowRequested, &app,
                         [present](const QUrl &) { present(); });
        QObject::connect(&instance, &SingleInstance::toggleRequested, &app,
                         [window] { window->setVisible(!window->isVisible()); });
        QObject::connect(&controller, &ApplicationController::closeWindowRequested,
                         window, &QQuickWindow::close);
    }

    const bool explicitTarget = independent || !parser.positionalArguments().isEmpty()
        || parser.isSet(selectOption);
    controller.start(target, selectName, explicitTarget);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller,
                     &ApplicationController::flushSession);

    if (parser.isSet(smokeOption))
        QTimer::singleShot(900, &app, [&app] { app.quit(); });

    return app.exec();
}
