#include "AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
QString cleanPath(QString path) {
    return QDir::cleanPath(path);
}

QString mk(const QString& path) {
    QDir().mkpath(path);
    return path;
}
}

QString AppPaths::root() {
    const QString overrideRoot = qEnvironmentVariable("MYQUANT_HOME").trimmed();
    if (!overrideRoot.isEmpty())
        return cleanPath(overrideRoot);

#ifdef Q_OS_MACOS
    return cleanPath(QDir::homePath() + "/Library/Application Support/MyQuant");
#elif defined(Q_OS_WIN)
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return cleanPath(base + "/MyQuant");
#else
    return cleanPath(QDir::homePath() + "/.local/share/MyQuant");
#endif
}

QString AppPaths::data() { return cleanPath(root() + "/data"); }
QString AppPaths::logs() { return cleanPath(root() + "/logs"); }
QString AppPaths::cache() { return cleanPath(root() + "/cache"); }
QString AppPaths::etf() { return cleanPath(root() + "/etf"); }
QString AppPaths::exports() { return cleanPath(root() + "/exports"); }

QString AppPaths::resources() {
    const QString exe = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        exe + "/../Resources/resources",
        exe + "/resources",
        QDir::currentPath() + "/resources",
        QDir::currentPath() + "/../resources",
    };
    for (const QString& candidate : candidates) {
        if (QFileInfo(candidate).isDir())
            return cleanPath(candidate);
    }
    return cleanPath(exe + "/resources");
}

QString AppPaths::resourceFile(const QString& relativePath) {
    return cleanPath(resources() + "/" + relativePath);
}

void AppPaths::ensureAll() {
    mk(root());
    mk(data());
    mk(logs());
    mk(cache());
    mk(etf());
    mk(exports());
}
