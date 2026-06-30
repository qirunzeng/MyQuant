#include "AppPaths.h"
#include "EtfRotationController.h"
#include "NotesController.h"
#include "SettingsController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("qirunzeng");
    QCoreApplication::setApplicationName("MyQuant");
    QCoreApplication::setApplicationVersion("0.1.0");

    AppPaths::ensureAll();

    SettingsController settings;
    EtfRotationController etf(&settings);
    NotesController notes;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("settingsController", &settings);
    engine.rootContext()->setContextProperty("etfController", &etf);
    engine.rootContext()->setContextProperty("notesController", &notes);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("MyQuant", "Main");
    return app.exec();
}
