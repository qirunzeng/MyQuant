#pragma once

#include <QObject>
#include <QProcess>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

class SettingsController;

class DividendController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList holdings READ holdings NOTIFY dataChanged)
    Q_PROPERTY(QVariantList events READ events NOTIFY dataChanged)
    Q_PROPERTY(QVariantList calendarDays READ calendarDays NOTIFY dataChanged)
    Q_PROPERTY(QVariantList forecasts READ forecasts NOTIFY dataChanged)
    Q_PROPERTY(QVariantList trades READ trades NOTIFY dataChanged)
    Q_PROPERTY(QVariantList adjustments READ adjustments NOTIFY dataChanged)
    Q_PROPERTY(QString activeMonth READ activeMonth NOTIFY activeMonthChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)

public:
    explicit DividendController(SettingsController* settings, QObject* parent = nullptr);
    ~DividendController() override;

    QVariantList holdings() const { return holdings_; }
    QVariantList events() const { return events_; }
    QVariantList calendarDays() const { return calendarDays_; }
    QVariantList forecasts() const { return forecasts_; }
    QVariantList trades() const { return trades_; }
    QVariantList adjustments() const { return adjustments_; }
    QString activeMonth() const { return activeMonth_; }
    QString statusMessage() const { return statusMessage_; }
    bool refreshing() const { return refreshing_; }

    Q_INVOKABLE void loadMonth(const QString& month);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap newHolding() const;
    Q_INVOKABLE QVariantMap newEvent() const;
    Q_INVOKABLE QVariantMap newTrade() const;
    Q_INVOKABLE bool saveHolding(const QVariantMap& row);
    Q_INVOKABLE bool archiveHolding(int id);
    Q_INVOKABLE bool saveEvent(const QVariantMap& row);
    Q_INVOKABLE bool archiveEvent(int id);
    Q_INVOKABLE bool saveTrade(const QVariantMap& row);
    Q_INVOKABLE bool updateTradeFee(int tradeId, double actualFee, const QString& note);
    Q_INVOKABLE double estimateTradeFee(const QString& market, const QString& instrumentType,
                                        const QString& side, double amount) const;
    Q_INVOKABLE QString feeRuleDescription(const QString& market, const QString& instrumentType) const;
    Q_INVOKABLE void refreshOnline();

signals:
    void dataChanged();
    void activeMonthChanged();
    void statusMessageChanged();
    void refreshingChanged();

private:
    bool ensureDatabase();
    bool execSql(const QString& sql);
    void loadData();
    void buildCalendar();
    void buildForecasts();
    void applyDueDividendAdjustments();
    void importFetchedEvents(const QString& path);
    void setStatus(const QString& message);
    void setRefreshing(bool value);

    SettingsController* settings_ = nullptr;
    QSqlDatabase db_;
    QProcess* process_ = nullptr;
    QVariantList holdings_;
    QVariantList events_;
    QVariantList calendarDays_;
    QVariantList forecasts_;
    QVariantList trades_;
    QVariantList adjustments_;
    QString activeMonth_;
    QString statusMessage_;
    QString fetchOutputPath_;
    bool refreshing_ = false;
};
