#include "../src/EtfRotationController.h"
#include "../src/NotesController.h"
#include "../src/SettingsController.h"
#include "../src/DividendController.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QTemporaryDir>

#include <cmath>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            qCritical().noquote() << "CHECK failed:" << #condition << "at" << __FILE__ << ':' << __LINE__; \
            return 1; \
        } \
    } while (false)

namespace {
bool closeEnough(double lhs, double rhs) {
    return std::abs(lhs - rhs) < 0.0001;
}
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    CHECK(dir.isValid());
    qputenv("MYQUANT_HOME", dir.path().toUtf8());

    SettingsController settings;
    CHECK(settings.save());

    qInfo() << "smoke: dividends";
    DividendController dividends(&settings);
    QVariantMap holding = dividends.newHolding();
    holding["market"] = "US";
    holding["symbol"] = "AAPL";
    holding["name"] = "Apple";
    holding["shares"] = 10;
    holding["cost_price"] = 100;
    holding["currency"] = "USD";
    CHECK(dividends.saveHolding(holding));
    CHECK(dividends.holdings().size() == 1);
    const int holdingId = dividends.holdings().first().toMap().value("id").toInt();

    qInfo() << "smoke: dividend buy";
    QVariantMap buy = dividends.newTrade();
    buy["holding_id"] = holdingId;
    buy["side"] = "买入";
    buy["shares"] = 5;
    buy["price"] = 110;
    buy["fee"] = 1;
    CHECK(dividends.saveTrade(buy));
    QVariantMap savedHolding = dividends.holdings().first().toMap();
    CHECK(closeEnough(savedHolding.value("shares").toDouble(), 15));
    CHECK(closeEnough(savedHolding.value("cost_price").toDouble(), 103.4));

    qInfo() << "smoke: dividend sell";
    QVariantMap sell = dividends.newTrade();
    sell["holding_id"] = holdingId;
    sell["side"] = "卖出";
    sell["shares"] = 3;
    sell["price"] = 120;
    sell["fee"] = 2;
    CHECK(dividends.saveTrade(sell));
    savedHolding = dividends.holdings().first().toMap();
    CHECK(closeEnough(savedHolding.value("shares").toDouble(), 12));
    CHECK(closeEnough(savedHolding.value("cost_price").toDouble(), 103.4));
    CHECK(dividends.trades().size() == 2);
    CHECK(closeEnough(dividends.trades().first().toMap().value("realized_pnl").toDouble(), 47.8));

    qInfo() << "smoke: dividend oversell and fees";
    QVariantMap oversell = sell;
    oversell["shares"] = 100;
    CHECK(!dividends.saveTrade(oversell));
    CHECK(closeEnough(dividends.holdings().first().toMap().value("shares").toDouble(), 12));
    CHECK(closeEnough(dividends.estimateTradeFee("A", "stock", "买入", 10000), 0.741));
    CHECK(closeEnough(dividends.estimateTradeFee("A", "stock", "卖出", 10000), 5.741));
    CHECK(closeEnough(dividends.estimateTradeFee("A", "etf", "买入", 10000), 0.5));
    CHECK(closeEnough(dividends.estimateTradeFee("A", "etf", "卖出", 10000), 0.5));
    CHECK(closeEnough(dividends.estimateTradeFee("A", "stock", "买入", 100), 0.3));
    CHECK(closeEnough(dividends.estimateTradeFee("A", "stock", "卖出", 100), 0.35));
    CHECK(closeEnough(dividends.estimateTradeFee("A", "etf", "买入", 100), 0.1));
    CHECK(closeEnough(dividends.estimateTradeFee("A", "etf", "卖出", 100), 0.1));
    CHECK(closeEnough(dividends.estimateTradeFee("H", "stock", "买入", 10000), 11.27));
    CHECK(closeEnough(dividends.estimateTradeFee("US", "stock", "买入", 10000), 0));

    int buyTradeId = 0;
    int sellTradeId = 0;
    for (const QVariant& value : dividends.trades()) {
        const QVariantMap trade = value.toMap();
        if (trade.value("side").toString() == "买入") buyTradeId = trade.value("id").toInt();
        if (trade.value("side").toString() == "卖出") sellTradeId = trade.value("id").toInt();
    }
    CHECK(dividends.updateTradeFee(buyTradeId, 2, "buy statement"));
    CHECK(closeEnough(dividends.holdings().first().toMap().value("cost_price").toDouble(), 103.4 + 1.0 / 12.0));
    CHECK(dividends.updateTradeFee(sellTradeId, 3, "sell statement"));
    CHECK(closeEnough(dividends.trades().first().toMap().value("realized_pnl").toDouble(), 46.8));
    CHECK(dividends.trades().first().toMap().value("fee_is_actual").toBool());

    qInfo() << "smoke: dividend adjustment";
    QVariantMap event = dividends.newEvent();
    event["market"] = "US";
    event["symbol"] = "AAPL";
    event["name"] = "Apple";
    event["ex_date"] = QDate::currentDate().toString("yyyy-MM-dd");
    event["pay_date"] = QDate::currentDate().addDays(7).toString("yyyy-MM-dd");
    event["amount_per_share"] = 1.25;
    event["currency"] = "USD";
    CHECK(dividends.saveEvent(event));
    CHECK(dividends.adjustments().size() == 1);
    CHECK(closeEnough(dividends.holdings().first().toMap().value("cost_price").toDouble(), 102.2333333333));
    dividends.refresh();
    CHECK(dividends.adjustments().size() == 1);
    CHECK(closeEnough(dividends.holdings().first().toMap().value("cost_price").toDouble(), 102.2333333333));
    for (const QVariant& value : dividends.calendarDays()) {
        const QDate date = QDate::fromString(value.toMap().value("date").toString(), "yyyy-MM-dd");
        CHECK(date.dayOfWeek() >= 1 && date.dayOfWeek() <= 5);
    }
    CHECK(dividends.archiveHolding(holdingId));
    CHECK(dividends.events().isEmpty());
    for (const QVariant& value : dividends.calendarDays())
        CHECK(value.toMap().value("events").toList().isEmpty());

    qInfo() << "smoke: notes";
    NotesController notes;
    QVariantMap draft = notes.createFromTemplate("交易复盘");
    draft["title"] = "Smoke Note";
    CHECK(notes.saveNote(draft));
    CHECK(!notes.notes().isEmpty());

    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    const QString currentMonth = QDate::currentDate().toString("yyyy-MM");
    notes.ensureMonth(currentMonth);
    QVariantMap marketDay = notes.createMarketDay(today);
    marketDay["market_view"] = "Smoke market view";
    CHECK(notes.saveMarketDay(marketDay));
    QVariantMap reviewTrade = notes.createTradeExecution(today);
    reviewTrade["symbol"] = "AAPL";
    reviewTrade["name"] = "Apple";
    reviewTrade["price"] = 123.45;
    reviewTrade["trade_reasons"] = "Smoke trade";
    CHECK(notes.saveTradeExecution(reviewTrade));
    QVariantMap weekly = notes.createPeriodSummary("week");
    weekly["total_return"] = -0.03;
    weekly["notes"] = "Smoke weekly review";
    CHECK(notes.savePeriodSummary(weekly));
    CHECK(!notes.marketDays().isEmpty());
    CHECK(!notes.tradeExecutions().isEmpty());
    bool foundWeekCard = false;
    for (const QVariant& value : notes.timelineCards()) {
        if (value.toMap().value("type").toString() == "week")
            foundWeekCard = true;
    }
    CHECK(foundWeekCard);

    settings.setTheme("dark");
    settings.setDataSources("em,tx");
    QVariantMap customFees = settings.feeSettings();
    customFees["aStockRate"] = 0.8;
    customFees["aStockMinimum"] = 0.4;
    settings.setFeeSettings(customFees);
    CHECK(settings.save());
    settings.setTheme("light");
    settings.setDataSources("sina");
    settings.load();
    CHECK(settings.theme() == "dark");
    CHECK(settings.dataSources() == "em,tx");
    CHECK(closeEnough(settings.feeSettings().value("aStockRate").toDouble(), 0.8));
    CHECK(closeEnough(settings.feeSettings().value("aStockMinimum").toDouble(), 0.4));

    qInfo() << "smoke: etf";
    qputenv("MYQUANT_ALLOW_DEMO_DATA", "1");
    EtfRotationController etf(&settings);
    CHECK(etf.runDefault(false, "20220101", "20231231", 4, 0.25, 0.07, 40000));
    CHECK(!etf.equity().isEmpty());
    CHECK(!etf.rankings().isEmpty());
    CHECK(!etf.summary().isEmpty());
    CHECK(etf.dataIssues().isEmpty());
    CHECK(!etf.advice().isEmpty());
    CHECK(!etf.pricePoints().isEmpty());
    CHECK(!etf.tradeCharts().isEmpty());
    const QString firstActualStart = etf.metrics().value("startDate").toString();
    CHECK(etf.runDefault(false, "20230101", "20231231", 4, 0.25, 0.07, 40000));
    CHECK(etf.metrics().value("startDate").toString() != firstActualStart);
    CHECK(etf.metrics().value("startDate").toString() >= QString("2023-01-01"));
    CHECK(etf.runDefault(false, "20990101", "20991231", 4, 0.25, 0.07, 40000) == false);
    CHECK(etf.equity().isEmpty());
    CHECK(etf.metrics().isEmpty());
    CHECK(etf.runDefault(false, "20220101", "20231231", 4, 0.25, 0.07, 40000));

    QVariantMap portfolio = etf.portfolio();
    QVariantList positions = portfolio.value("positions").toList();
    if (!positions.isEmpty()) {
        QVariantMap first = positions.first().toMap();
        const QString disabledCode = first.value("code").toString();
        first["enabled"] = false;
        positions[0] = first;
        CHECK(etf.savePortfolio(portfolio.value("availableCash").toDouble(), positions));
        CHECK(etf.runDefault(false, "20220101", "20231231", 4, 0.25, 0.07, 40000));
        for (const QVariant& value : etf.rankings())
            CHECK(value.toMap().value("symbol").toString() != disabledCode);
    }
    portfolio = etf.portfolio();
    positions = portfolio.value("positions").toList();
    if (positions.size() > 1) {
        const QString removedCode = positions.last().toMap().value("code").toString();
        positions.removeLast();
        CHECK(etf.savePortfolio(portfolio.value("availableCash").toDouble(), positions));
        etf.refreshPortfolio();
        bool foundRemoved = false;
        for (const QVariant& value : etf.portfolio().value("positions").toList()) {
            if (value.toMap().value("code").toString() == removedCode)
                foundRemoved = true;
        }
        CHECK(!foundRemoved);
    }

    qInfo() << "smoke: complete";
    return 0;
}
