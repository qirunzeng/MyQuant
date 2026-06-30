#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class NotesController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList notes READ notes NOTIFY notesChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit NotesController(QObject* parent = nullptr);

    QVariantList notes() const { return notes_; }
    QString statusMessage() const { return statusMessage_; }

    Q_INVOKABLE void refresh(const QString& query = QString());
    Q_INVOKABLE QVariantMap createFromTemplate(const QString& templateName);
    Q_INVOKABLE bool saveNote(const QVariantMap& note);
    Q_INVOKABLE bool deleteNote(int id);
    Q_INVOKABLE bool toggleFavorite(int id);
    Q_INVOKABLE bool toggleArchive(int id);
    Q_INVOKABLE bool exportNote(int id);

signals:
    void notesChanged();
    void statusMessageChanged();

private:
    bool ensureDatabase();
    void setStatus(const QString& message);
    QVariantMap rowToNote(const QSqlQuery& query) const;
    QString noteMarkdown(const QVariantMap& note) const;

    QSqlDatabase db_;
    QVariantList notes_;
    QString statusMessage_;
};
