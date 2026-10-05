#include "AppPaths.h"
#include "EtfRotationController.h"
#include "NotesController.h"
#include "SettingsController.h"
#include "DividendController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#ifdef MYQUANT_UI_VERIFY
#include <QDir>
#include <QTemporaryDir>
#include <QTimer>
#include <QQuickWindow>
#endif

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("qirunzeng");
    QCoreApplication::setApplicationName("MyQuant");
    QCoreApplication::setApplicationVersion(MYQUANT_VERSION);

#ifdef MYQUANT_UI_VERIFY
    QTemporaryDir isolatedData;
    if (!isolatedData.isValid()) return 1;
    qputenv("MYQUANT_HOME", isolatedData.path().toUtf8());
#endif

    AppPaths::ensureAll();

    SettingsController settings;
    EtfRotationController etf(&settings);
    NotesController notes;
    DividendController dividends(&settings);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("settingsController", &settings);
    engine.rootContext()->setContextProperty("etfController", &etf);
    engine.rootContext()->setContextProperty("notesController", &notes);
    engine.rootContext()->setContextProperty("dividendController", &dividends);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("MyQuant", "Main");
#ifdef MYQUANT_UI_VERIFY
    if (engine.rootObjects().isEmpty()) return 1;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) return 1;
    const QString output = qEnvironmentVariable("MYQUANT_UI_CAPTURE_DIR");
    if (output.isEmpty() || !QDir().mkpath(output)) return 1;
    int frame = 0;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &app, [&] {
        if (frame > 0) {
            const auto image = window->grabWindow();
            if (image.isNull() || !image.save(output + "/" + QString::number(frame - 1) + ".png")) {
                app.exit(1); return;
            }
        }
        if (frame == 16) { app.quit(); return; }
        settings.setTheme(frame >= 8 ? "dark" : "light");
        window->setWidth((frame / 4) % 2 == 0 ? 900 : 1440);
        window->setHeight(900);
        window->setProperty("activePage", frame % 4);
        ++frame;
    });
    timer.start(350);
#endif
    return app.exec();
}
