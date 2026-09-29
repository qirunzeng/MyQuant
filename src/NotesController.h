#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QSqlQuery;

class NotesController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList notes READ notes NOTIFY notesChanged)
    Q_PROPERTY(QVariantList marketDays READ marketDays NOTIFY reviewDataChanged)
    Q_PROPERTY(QVariantList watchlistItems READ watchlistItems NOTIFY reviewDataChanged)
    Q_PROPERTY(QVariantList tradeExecutions READ tradeExecutions NOTIFY reviewDataChanged)
    Q_PROPERTY(QVariantList periodSummaries READ periodSummaries NOTIFY reviewDataChanged)
    Q_PROPERTY(QVariantList timelineCards READ timelineCards NOTIFY reviewDataChanged)
    Q_PROPERTY(QString activeMonth READ activeMonth NOTIFY activeMonthChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit NotesController(QObject* parent = nullptr);

    QVariantList notes() const { return notes_; }
    QVariantList marketDays() const { return marketDays_; }
    QVariantList watchlistItems() const { return watchlistItems_; }
    QVariantList tradeExecutions() const { return tradeExecutions_; }
    QVariantList periodSummaries() const { return periodSummaries_; }
    QVariantList timelineCards() const { return timelineCards_; }
    QString activeMonth() const { return activeMonth_; }
    QString statusMessage() const { return statusMessage_; }

    Q_INVOKABLE void refresh(const QString& query = QString());
    Q_INVOKABLE void loadMonth(const QString& yearMonth);
    Q_INVOKABLE void ensureMonth(const QString& yearMonth);

    Q_INVOKABLE QVariantMap createFromTemplate(const QString& templateName);
    Q_INVOKABLE bool saveNote(const QVariantMap& note);
    Q_INVOKABLE bool deleteNote(int id);
    Q_INVOKABLE bool toggleFavorite(int id);
    Q_INVOKABLE bool toggleArchive(int id);
    Q_INVOKABLE bool exportNote(int id);

    Q_INVOKABLE QVariantMap createMarketDay(const QString& date = QString()) const;
    Q_INVOKABLE QVariantMap createWatchlistItem(const QString& date = QString()) const;
    Q_INVOKABLE QVariantMap createTradeExecution(const QString& date = QString()) const;
    Q_INVOKABLE QVariantMap createPeriodSummary(const QString& periodType, const QString& periodKey = QString()) const;

    Q_INVOKABLE bool saveMarketDay(const QVariantMap& row);
    Q_INVOKABLE bool saveWatchlistItem(const QVariantMap& row);
    Q_INVOKABLE bool saveTradeExecution(const QVariantMap& row);
    Q_INVOKABLE bool savePeriodSummary(const QVariantMap& row);

    Q_INVOKABLE bool archiveMarketDay(int id);
    Q_INVOKABLE bool archiveWatchlistItem(int id);
    Q_INVOKABLE bool archiveTradeExecution(int id);
    Q_INVOKABLE bool archivePeriodSummary(int id);
    Q_INVOKABLE bool exportMonth(const QString& yearMonth);

signals:
    void notesChanged();
    void reviewDataChanged();
    void activeMonthChanged();
    void statusMessageChanged();

private:
    bool ensureDatabase();
    bool ensureReviewSchema();
    bool backupDatabaseForMigration(int fromVersion);
    bool execSql(const QString& sql);
    int schemaVersion() const;
    bool setSchemaVersion(int version);
    void setStatus(const QString& message);

    QVariantMap rowToNote(const QSqlQuery& query) const;
    QVariantMap rowToMap(const QSqlQuery& query) const;
    QString noteMarkdown(const QVariantMap& note) const;
    QString monthMarkdown(const QString& yearMonth) const;
    bool archiveRow(const QString& table, int id, const QString& label);
    bool saveRow(const QString& table, const QStringList& fields, QVariantMap row, const QString& label);
    void loadReviewTables();
    QVariantList buildTimelineCards() const;

    QSqlDatabase db_;
    QVariantList notes_;
    QVariantList marketDays_;
    QVariantList watchlistItems_;
    QVariantList tradeExecutions_;
    QVariantList periodSummaries_;
    QVariantList timelineCards_;
    QString activeMonth_;
    QString statusMessage_;
};
