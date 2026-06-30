#pragma once

#include "SettingsController.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class EtfRotationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY resultChanged)
    Q_PROPERTY(QVariantList equity READ equity NOTIFY resultChanged)
    Q_PROPERTY(QVariantList trades READ trades NOTIFY resultChanged)
    Q_PROPERTY(QVariantList rankings READ rankings NOTIFY resultChanged)
    Q_PROPERTY(QVariantList advice READ advice NOTIFY resultChanged)
    Q_PROPERTY(QVariantList pricePoints READ pricePoints NOTIFY resultChanged)
    Q_PROPERTY(QVariantList tradeCharts READ tradeCharts NOTIFY resultChanged)
    Q_PROPERTY(QVariantList dataIssues READ dataIssues NOTIFY resultChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY resultChanged)
    Q_PROPERTY(QVariantList universe READ universe NOTIFY universeChanged)
    Q_PROPERTY(QVariantMap portfolio READ portfolio NOTIFY portfolioChanged)
    Q_PROPERTY(QString reportMarkdown READ reportMarkdown NOTIFY resultChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit EtfRotationController(SettingsController* settings = nullptr, QObject* parent = nullptr);

    bool running() const { return running_; }
    QVariantMap metrics() const { return metrics_; }
    QVariantList equity() const { return equity_; }
    QVariantList trades() const { return trades_; }
    QVariantList rankings() const { return rankings_; }
    QVariantList advice() const { return advice_; }
    QVariantList pricePoints() const { return pricePoints_; }
    QVariantList tradeCharts() const { return tradeCharts_; }
    QVariantList dataIssues() const { return dataIssues_; }
    QVariantMap summary() const { return summary_; }
    QVariantList universe() const { return universe_; }
    QVariantMap portfolio() const { return portfolio_; }
    QString reportMarkdown() const { return reportMarkdown_; }
    QString statusMessage() const { return statusMessage_; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool runDefault(bool updateData, const QString& startDate, const QString& endDate,
                                int holdNum, double targetCashRatio, double stopLossRate, double realCash);
    Q_INVOKABLE bool saveUniverse(const QVariantList& rows);
    Q_INVOKABLE void refreshPortfolio();
    Q_INVOKABLE bool savePortfolio(double availableCash, const QVariantList& positions);
    Q_INVOKABLE bool openEtfFolder();

signals:
    void runningChanged();
    void resultChanged();
    void universeChanged();
    void portfolioChanged();
    void statusMessageChanged();

private:
    struct Bar {
        QString code;
        QString date;
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
        double vol = 0.0;
        double amount = 0.0;
        double momentum = 0.0;
        double stdScore = 0.0;
        double cvScore = 0.0;
        double ret20 = 0.0;
        double ret60 = 0.0;
        double ret120 = 0.0;
        double ma120 = 0.0;
        bool trendPass = false;
        double dualMomentum = 0.0;
        double combinedMomentum = 0.0;
    };

    void setRunning(bool value);
    void setStatus(const QString& message);
    bool ensureSeed();
    QVariantList loadUniverseRows() const;
    QList<Bar> loadBarsFromPath(const QString& path) const;
    QList<Bar> loadBars() const;
    QVariantMap loadPortfolio() const;
    bool importLegacyBarsIfAvailable();
    QVariantList collectDataIssues(const QList<Bar>& bars) const;
    bool barsLookSuspicious(const QList<Bar>& bars) const;
    bool hasLocalDataCoverage(const QList<Bar>& bars, const QString& startDate, const QString& endDate) const;
    bool fetchLiveData(const QString& startDate, const QString& endDate);
    bool generateDemoData(const QString& startDate, const QString& endDate);
    bool writeBars(const QList<Bar>& bars) const;
    QVariantMap runBacktest(QList<Bar> bars, const QString& startDate, const QString& endDate,
                            int holdNum, double targetCashRatio,
                            double stopLossRate, double initialCapital);
    QVariantList buildAdvice(const QVariantList& rankings, const QHash<QString, Bar>& latestBars,
                             double targetCashRatio, int holdNum) const;
    QVariantList buildPricePoints(const QVariantList& trades) const;
    QVariantList buildTradeCharts(const QHash<QString, QVector<Bar>>& byCode, const QVariantList& trades,
                                  const QString& startDate, const QString& endDate) const;
    QVariantMap buildSummary(const QVariantMap& metrics, const QVariantList& rankings,
                             const QVariantList& advice, const QVariantList& dataIssues) const;
    static QString normalizeCode(QString code);
    static QString displayCode(QString code);
    static double weightedMomentum(const QVector<double>& closes, int index);

    SettingsController* settings_ = nullptr;
    bool running_ = false;
    QVariantMap metrics_;
    QVariantList equity_;
    QVariantList trades_;
    QVariantList rankings_;
    QVariantList advice_;
    QVariantList pricePoints_;
    QVariantList tradeCharts_;
    QVariantList dataIssues_;
    QVariantMap summary_;
    QVariantList universe_;
    QVariantMap portfolio_;
    QString reportMarkdown_;
    QString statusMessage_;
};
