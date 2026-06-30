#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class SettingsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString pythonPath READ pythonPath WRITE setPythonPath NOTIFY settingsChanged)
    Q_PROPERTY(QString dataSources READ dataSources WRITE setDataSources NOTIFY settingsChanged)
    Q_PROPERTY(QString llmBaseUrl READ llmBaseUrl WRITE setLlmBaseUrl NOTIFY settingsChanged)
    Q_PROPERTY(QString llmApiKey READ llmApiKey WRITE setLlmApiKey NOTIFY settingsChanged)
    Q_PROPERTY(QString llmModel READ llmModel WRITE setLlmModel NOTIFY settingsChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY settingsChanged)
    Q_PROPERTY(QString dataRoot READ dataRoot CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit SettingsController(QObject* parent = nullptr);

    QString pythonPath() const { return pythonPath_; }
    QString dataSources() const { return dataSources_; }
    QString llmBaseUrl() const { return llmBaseUrl_; }
    QString llmApiKey() const { return llmApiKey_; }
    QString llmModel() const { return llmModel_; }
    QString theme() const { return theme_; }
    QString dataRoot() const;
    QString statusMessage() const { return statusMessage_; }

    void setPythonPath(const QString& value);
    void setDataSources(const QString& value);
    void setLlmBaseUrl(const QString& value);
    void setLlmApiKey(const QString& value);
    void setLlmModel(const QString& value);
    void setTheme(const QString& value);

    Q_INVOKABLE void load();
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool clearCache();
    Q_INVOKABLE bool openDataRoot();
    Q_INVOKABLE QVariantMap asMap() const;

signals:
    void settingsChanged();
    void statusMessageChanged();

private:
    QString configPath() const;
    void setStatus(const QString& message);
    static QString defaultPythonPath();

    QString pythonPath_;
    QString dataSources_ = "em,tx,sina";
    QString llmBaseUrl_;
    QString llmApiKey_;
    QString llmModel_ = "gpt-4.1-mini";
    QString theme_ = "dark";
    QString statusMessage_;
};
