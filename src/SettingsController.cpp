#include "SettingsController.h"

#include "AppPaths.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>

SettingsController::SettingsController(QObject* parent) : QObject(parent) {
    AppPaths::ensureAll();
    load();
}

QString SettingsController::defaultPythonPath() {
    const QStringList candidates = {
        "/opt/homebrew/Caskroom/miniconda/base/envs/etf/bin/python",
        "/opt/homebrew/bin/python3",
        "/usr/bin/python3",
        "python3",
    };
    for (const QString& candidate : candidates) {
        if (candidate == "python3" || QFile::exists(candidate))
            return candidate;
    }
    return "python3";
}

QString SettingsController::configPath() const {
    return AppPaths::data() + "/settings.json";
}

QString SettingsController::dataRoot() const {
    return AppPaths::root();
}

void SettingsController::setStatus(const QString& message) {
    if (statusMessage_ == message)
        return;
    statusMessage_ = message;
    emit statusMessageChanged();
}

void SettingsController::setPythonPath(const QString& value) {
    if (pythonPath_ == value)
        return;
    pythonPath_ = value.trimmed();
    emit settingsChanged();
}

void SettingsController::setDataSources(const QString& value) {
    if (dataSources_ == value)
        return;
    dataSources_ = value.trimmed().isEmpty() ? "em,tx,sina" : value.trimmed();
    emit settingsChanged();
}

void SettingsController::setLlmBaseUrl(const QString& value) {
    if (llmBaseUrl_ == value)
        return;
    llmBaseUrl_ = value.trimmed();
    emit settingsChanged();
}

void SettingsController::setLlmApiKey(const QString& value) {
    if (llmApiKey_ == value)
        return;
    llmApiKey_ = value.trimmed();
    emit settingsChanged();
}

void SettingsController::setLlmModel(const QString& value) {
    if (llmModel_ == value)
        return;
    llmModel_ = value.trimmed().isEmpty() ? "gpt-4.1-mini" : value.trimmed();
    emit settingsChanged();
}

void SettingsController::setTheme(const QString& value) {
    if (theme_ == value)
        return;
    theme_ = value.trimmed().isEmpty() ? "dark" : value.trimmed();
    emit settingsChanged();
}

void SettingsController::load() {
    pythonPath_ = defaultPythonPath();

    QFile file(configPath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
        pythonPath_ = obj.value("pythonPath").toString(pythonPath_);
        dataSources_ = obj.value("dataSources").toString(dataSources_);
        llmBaseUrl_ = obj.value("llmBaseUrl").toString(llmBaseUrl_);
        llmApiKey_ = obj.value("llmApiKey").toString(llmApiKey_);
        llmModel_ = obj.value("llmModel").toString(llmModel_);
        theme_ = obj.value("theme").toString(theme_);
    }
    setStatus("设置已载入");
    emit settingsChanged();
}

bool SettingsController::save() {
    AppPaths::ensureAll();
    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly)) {
        setStatus("设置保存失败");
        return false;
    }
    const QJsonObject obj{
        {"pythonPath", pythonPath_},
        {"dataSources", dataSources_},
        {"llmBaseUrl", llmBaseUrl_},
        {"llmApiKey", llmApiKey_},
        {"llmModel", llmModel_},
        {"theme", theme_},
        {"savedAt", QDateTime::currentDateTime().toString(Qt::ISODate)},
    };
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    const bool ok = file.commit();
    setStatus(ok ? "设置已保存" : "设置保存失败");
    return ok;
}

bool SettingsController::clearCache() {
    QDir dir(AppPaths::cache());
    const bool ok = dir.removeRecursively() && QDir().mkpath(AppPaths::cache());
    setStatus(ok ? "缓存已清理" : "缓存清理失败");
    return ok;
}

bool SettingsController::openDataRoot() {
    const bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::root()));
    setStatus(ok ? "已打开数据目录" : "无法打开数据目录");
    return ok;
}

QVariantMap SettingsController::asMap() const {
    return {
        {"pythonPath", pythonPath_},
        {"dataSources", dataSources_},
        {"llmBaseUrl", llmBaseUrl_},
        {"llmApiKey", llmApiKey_},
        {"llmModel", llmModel_},
        {"theme", theme_},
        {"dataRoot", AppPaths::root()},
    };
}
