#include "DividendController.h"

#include "AppPaths.h"
#include "SettingsController.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {
QString nowIso() { return QDateTime::currentDateTime().toString(Qt::ISODate); }

QString normalizedMonth(const QString& value) {
    const QString month = value.trimmed().left(7);
    return QDate::fromString(month + "-01", "yyyy-MM-dd").isValid()
        ? month : QDate::currentDate().toString("yyyy-MM");
}

QString text(const QVariantMap& row, const QString& key) {
    return row.value(key).toString().trimmed();
}

double number(const QVariantMap& row, const QString& key) {
    bool ok = false;
    const double value = row.value(key).toString().trimmed().remove(',').toDouble(&ok);
    return ok ? value : row.value(key).toDouble();
}

QVariantMap queryRow(const QSqlQuery& query) {
    QVariantMap row;
    const QSqlRecord record = query.record();
    for (int i = 0; i < record.count(); ++i)
        row.insert(record.fieldName(i), query.value(i));
    return row;
}
}

DividendController::DividendController(SettingsController* settings, QObject* parent)
    : QObject(parent), settings_(settings), activeMonth_(QDate::currentDate().toString("yyyy-MM")) {
    AppPaths::ensureAll();
    ensureDatabase();
    loadData();
}

DividendController::~DividendController() {
    if (process_ && process_->state() != QProcess::NotRunning) {
        process_->terminate();
        process_->waitForFinished(1000);
    }
}

void DividendController::setStatus(const QString& message) {
    if (statusMessage_ == message)
        return;
    statusMessage_ = message;
    emit statusMessageChanged();
}

void DividendController::setRefreshing(bool value) {
    if (refreshing_ == value)
        return;
    refreshing_ = value;
    emit refreshingChanged();
}

bool DividendController::execSql(const QString& sql) {
    QSqlQuery query(db_);
    if (query.exec(sql))
        return true;
    setStatus("股息数据库错误：" + query.lastError().text());
    return false;
}

bool DividendController::ensureDatabase() {
    const QString connection = "myquant_dividends";
    db_ = QSqlDatabase::contains(connection) ? QSqlDatabase::database(connection)
                                             : QSqlDatabase::addDatabase("QSQLITE", connection);
    db_.setDatabaseName(AppPaths::data() + "/dividends.db");
    if (!db_.open()) {
        setStatus("无法打开股息数据库：" + db_.lastError().text());
        return false;
    }
    if (!execSql("CREATE TABLE IF NOT EXISTS dividend_holdings ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, market TEXT NOT NULL, symbol TEXT NOT NULL,"
                   "name TEXT, shares REAL DEFAULT 0, cost_price REAL DEFAULT 0, currency TEXT,"
                   "instrument_type TEXT DEFAULT 'stock', auto_adjust_from TEXT,"
                   "archived INTEGER DEFAULT 0, created_at TEXT NOT NULL, updated_at TEXT NOT NULL,"
                   "UNIQUE(market, symbol))") ||
        !execSql("CREATE TABLE IF NOT EXISTS dividend_events ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, market TEXT NOT NULL, symbol TEXT NOT NULL,"
                   "name TEXT, declaration_date TEXT, record_date TEXT, ex_date TEXT, pay_date TEXT,"
                   "amount_per_share REAL DEFAULT 0, currency TEXT, event_type TEXT DEFAULT '现金分红',"
                   "source TEXT DEFAULT '手工', announced INTEGER DEFAULT 1, notes TEXT, archived INTEGER DEFAULT 0,"
                   "created_at TEXT NOT NULL, updated_at TEXT NOT NULL,"
                   "UNIQUE(market, symbol, ex_date, pay_date, amount_per_share))") ||
        !execSql("CREATE TABLE IF NOT EXISTS dividend_trades ("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT, holding_id INTEGER NOT NULL, market TEXT NOT NULL,"
                 "symbol TEXT NOT NULL, name TEXT, trade_date TEXT NOT NULL, side TEXT NOT NULL,"
                 "shares REAL NOT NULL, price REAL NOT NULL, fee REAL DEFAULT 0, currency TEXT,"
                 "instrument_type TEXT DEFAULT 'stock', fee_rate_per_ten_thousand REAL DEFAULT 0,"
                 "fee_is_actual INTEGER DEFAULT 0, realized_pnl REAL DEFAULT 0, note TEXT, created_at TEXT NOT NULL)") ||
        !execSql("CREATE TABLE IF NOT EXISTS dividend_adjustments ("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT, event_id INTEGER NOT NULL, holding_id INTEGER NOT NULL,"
                 "market TEXT NOT NULL, symbol TEXT NOT NULL, name TEXT, ex_date TEXT NOT NULL, shares REAL NOT NULL,"
                 "amount_per_share REAL NOT NULL, gross_amount REAL NOT NULL, old_cost_price REAL NOT NULL,"
                 "new_cost_price REAL NOT NULL, currency TEXT, created_at TEXT NOT NULL,"
                 "UNIQUE(event_id, holding_id))") ||
        !execSql("CREATE INDEX IF NOT EXISTS idx_dividend_events_dates ON dividend_events(ex_date, record_date, pay_date)") ||
        !execSql("CREATE INDEX IF NOT EXISTS idx_dividend_trades_date ON dividend_trades(trade_date DESC)"))
        return false;

    auto hasColumn = [this](const QString& table, const QString& name) {
        QSqlQuery query(db_);
        if (!query.exec("PRAGMA table_info(" + table + ")"))
            return false;
        while (query.next())
            if (query.value(1).toString() == name)
                return true;
        return false;
    };
    const bool hasAutoAdjustFrom = hasColumn("dividend_holdings", "auto_adjust_from");
    if (!hasAutoAdjustFrom && !execSql("ALTER TABLE dividend_holdings ADD COLUMN auto_adjust_from TEXT"))
        return false;
    if (!hasColumn("dividend_holdings", "instrument_type") &&
        !execSql("ALTER TABLE dividend_holdings ADD COLUMN instrument_type TEXT DEFAULT 'stock'"))
        return false;
    if (!hasColumn("dividend_trades", "instrument_type") &&
        !execSql("ALTER TABLE dividend_trades ADD COLUMN instrument_type TEXT DEFAULT 'stock'"))
        return false;
    if (!hasColumn("dividend_trades", "fee_rate_per_ten_thousand") &&
        !execSql("ALTER TABLE dividend_trades ADD COLUMN fee_rate_per_ten_thousand REAL DEFAULT 0"))
        return false;
    if (!hasColumn("dividend_trades", "fee_is_actual") &&
        !execSql("ALTER TABLE dividend_trades ADD COLUMN fee_is_actual INTEGER DEFAULT 0"))
        return false;

    QSqlQuery initialize(db_);
    initialize.prepare("UPDATE dividend_holdings SET auto_adjust_from=? WHERE auto_adjust_from IS NULL OR auto_adjust_from='' ");
    initialize.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    if (!initialize.exec()) {
        setStatus("无法初始化分红复权起始日：" + initialize.lastError().text());
        return false;
    }
    return true;
}

QVariantMap DividendController::newHolding() const {
    return {{"id", 0}, {"market", "A"}, {"symbol", ""}, {"name", ""},
            {"instrument_type", "stock"}, {"shares", 0}, {"cost_price", 0}, {"currency", "CNY"}};
}

QVariantMap DividendController::newEvent() const {
    return {{"id", 0}, {"market", "A"}, {"symbol", ""}, {"name", ""},
            {"declaration_date", ""}, {"record_date", ""}, {"ex_date", ""}, {"pay_date", ""},
            {"amount_per_share", 0}, {"currency", "CNY"}, {"event_type", "现金分红"},
            {"source", "手工"}, {"announced", 1}, {"notes", ""}};
}

QVariantMap DividendController::newTrade() const {
    return {{"holding_id", 0}, {"market", "A"}, {"symbol", ""}, {"name", ""},
            {"instrument_type", "stock"},
            {"trade_date", QDate::currentDate().toString("yyyy-MM-dd")}, {"side", "买入"},
            {"shares", 0}, {"price", 0}, {"fee", 0}, {"fee_is_actual", false},
            {"currency", "CNY"}, {"note", ""}};
}

bool DividendController::saveHolding(const QVariantMap& row) {
    const QString market = text(row, "market").toUpper();
    const QString symbol = text(row, "symbol").toUpper();
    const QString instrumentType = text(row, "instrument_type") == "etf" ? "etf" : "stock";
    if (!QStringList{"A", "H", "US"}.contains(market) || symbol.isEmpty()) {
        setStatus("请填写有效市场和代码");
        return false;
    }
    QSqlQuery q(db_);
    if (row.value("id").toInt() > 0) {
        q.prepare("UPDATE dividend_holdings SET market=?,symbol=?,name=?,instrument_type=?,shares=?,cost_price=?,currency=?,updated_at=? WHERE id=?");
    } else {
        q.prepare("INSERT INTO dividend_holdings (market,symbol,name,instrument_type,shares,cost_price,currency,auto_adjust_from,created_at,updated_at) "
                  "VALUES (?,?,?,?,?,?,?,?,?,?) ON CONFLICT(market,symbol) DO UPDATE SET name=excluded.name,instrument_type=excluded.instrument_type,shares=excluded.shares,"
                  "cost_price=excluded.cost_price,currency=excluded.currency,updated_at=excluded.updated_at");
    }
    q.addBindValue(market); q.addBindValue(symbol); q.addBindValue(text(row, "name"));
    q.addBindValue(instrumentType);
    q.addBindValue(number(row, "shares")); q.addBindValue(number(row, "cost_price"));
    q.addBindValue(text(row, "currency").toUpper());
    if (row.value("id").toInt() > 0) {
        q.addBindValue(nowIso()); q.addBindValue(row.value("id").toInt());
    } else {
        q.addBindValue(QDate::currentDate().toString("yyyy-MM-dd")); q.addBindValue(nowIso()); q.addBindValue(nowIso());
    }
    if (!q.exec()) { setStatus("持仓保存失败：" + q.lastError().text()); return false; }
    setStatus("持仓已保存"); loadData(); return true;
}

bool DividendController::archiveHolding(int id) {
    QSqlQuery q(db_); q.prepare("UPDATE dividend_holdings SET archived=1,updated_at=? WHERE id=?");
    q.addBindValue(nowIso()); q.addBindValue(id);
    if (!q.exec()) { setStatus("持仓删除失败"); return false; }
    setStatus("持仓已移除，历史股息仍保留"); loadData(); return true;
}

bool DividendController::saveEvent(const QVariantMap& row) {
    const QString market = text(row, "market").toUpper();
    const QString symbol = text(row, "symbol").toUpper();
    if (!QStringList{"A", "H", "US"}.contains(market) || symbol.isEmpty()) {
        setStatus("请填写事件的市场和代码"); return false;
    }
    QSqlQuery q(db_);
    if (row.value("id").toInt() > 0)
        q.prepare("UPDATE dividend_events SET market=?,symbol=?,name=?,declaration_date=?,record_date=?,ex_date=?,pay_date=?,"
                  "amount_per_share=?,currency=?,event_type=?,source=?,announced=?,notes=?,updated_at=? WHERE id=?");
    else
        q.prepare("INSERT INTO dividend_events (market,symbol,name,declaration_date,record_date,ex_date,pay_date,amount_per_share,"
                  "currency,event_type,source,announced,notes,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    const QVariantList values{market, symbol, text(row,"name"), text(row,"declaration_date"), text(row,"record_date"),
        text(row,"ex_date"), text(row,"pay_date"), number(row,"amount_per_share"), text(row,"currency").toUpper(),
        text(row,"event_type"), text(row,"source").isEmpty() ? "手工" : text(row,"source"),
        row.value("announced", true).toBool() ? 1 : 0, text(row,"notes")};
    for (const QVariant& value : values) q.addBindValue(value);
    if (row.value("id").toInt() > 0) { q.addBindValue(nowIso()); q.addBindValue(row.value("id").toInt()); }
    else { q.addBindValue(nowIso()); q.addBindValue(nowIso()); }
    if (!q.exec()) { setStatus("股息事件保存失败：" + q.lastError().text()); return false; }
    setStatus("股息事件已保存"); loadData(); return true;
}

bool DividendController::archiveEvent(int id) {
    QSqlQuery q(db_); q.prepare("UPDATE dividend_events SET archived=1,updated_at=? WHERE id=?");
    q.addBindValue(nowIso()); q.addBindValue(id);
    if (!q.exec()) { setStatus("事件删除失败"); return false; }
    setStatus("事件已归档"); loadData(); return true;
}

double DividendController::estimateTradeFee(const QString& market, const QString& instrumentType,
                                            const QString& side, double amount) const {
    if (amount <= 0)
        return 0;
    const QString normalizedMarket = market.trimmed().toUpper();
    const QString normalizedType = instrumentType.trimmed().toLower() == "etf" ? "etf" : "stock";
    const bool selling = side.trimmed() == "卖出";
    const QVariantMap fees = settings_ ? settings_->feeSettings() : QVariantMap{};
    auto configuredFee = [&](const QString& rateKey, const QString& minimumKey) {
        return qMax(fees.value(minimumKey).toDouble(),
                    amount * fees.value(rateKey).toDouble() / 10000.0);
    };
    if (normalizedMarket == "A") {
        const bool etf = normalizedType == "etf";
        const double commission = configuredFee(etf ? "aEtfRate" : "aStockRate",
                                                etf ? "aEtfMinimum" : "aStockMinimum");
        const double stamp = selling && !etf
            ? amount * fees.value("aSellStampRate").toDouble() / 10000.0 : 0.0;
        return commission + stamp;
    }
    if (normalizedMarket == "H")
        return configuredFee("hkRate", "hkMinimum");
    return configuredFee("usRate", "usMinimum");
}

QString DividendController::feeRuleDescription(const QString& market, const QString& instrumentType) const {
    const QString normalized = market.trimmed().toUpper();
    const bool etf = instrumentType.trimmed().toLower() == "etf";
    const QVariantMap fees = settings_ ? settings_->feeSettings() : QVariantMap{};
    if (normalized == "A") {
        const double rate = fees.value(etf ? "aEtfRate" : "aStockRate").toDouble();
        const double minimum = fees.value(etf ? "aEtfMinimum" : "aStockMinimum").toDouble();
        QString result = QString("默认：%1每万元，最低%2元").arg(rate).arg(minimum);
        if (!etf)
            result += QString("；卖出印花税%1每万元").arg(fees.value("aSellStampRate").toDouble());
        return result + "。交割后请补录实际总费用。";
    }
    const QString prefix = normalized == "H" ? "港股微牛" : "美股微牛";
    const QString rateKey = normalized == "H" ? "hkRate" : "usRate";
    const QString minimumKey = normalized == "H" ? "hkMinimum" : "usMinimum";
    return QString("%1默认：%2每万元，最低%3；法定及监管收费可能变化，请以交割单实际总费用覆盖。")
        .arg(prefix).arg(fees.value(rateKey).toDouble()).arg(fees.value(minimumKey).toDouble());
}

bool DividendController::saveTrade(const QVariantMap& row) {
    const int holdingId = row.value("holding_id").toInt();
    const QString side = text(row, "side");
    const double quantity = number(row, "shares");
    const double price = number(row, "price");
    const double fee = qMax(0.0, number(row, "fee"));
    const QDate tradeDate = QDate::fromString(text(row, "trade_date"), "yyyy-MM-dd");
    if (holdingId <= 0 || !QStringList{"买入", "卖出"}.contains(side) || quantity <= 0 || price <= 0 || !tradeDate.isValid()) {
        setStatus("请填写有效的交易日期、方向、数量和价格");
        return false;
    }

    QSqlQuery holdingQuery(db_);
    holdingQuery.prepare("SELECT market,symbol,name,shares,cost_price,currency,instrument_type FROM dividend_holdings WHERE id=? AND archived=0");
    holdingQuery.addBindValue(holdingId);
    if (!holdingQuery.exec() || !holdingQuery.next()) {
        setStatus("找不到对应持仓");
        return false;
    }
    const QString market = holdingQuery.value(0).toString();
    const QString symbol = holdingQuery.value(1).toString();
    const QString name = holdingQuery.value(2).toString();
    const double oldShares = holdingQuery.value(3).toDouble();
    const double oldCost = holdingQuery.value(4).toDouble();
    const QString currency = holdingQuery.value(5).toString();
    const QString instrumentType = holdingQuery.value(6).toString() == "etf" ? "etf" : "stock";
    if (side == "卖出" && quantity > oldShares + 1e-8) {
        setStatus("卖出数量不能超过当前持仓");
        return false;
    }

    double newShares = oldShares;
    double newCost = oldCost;
    double realizedPnl = 0;
    if (side == "买入") {
        newShares += quantity;
        newCost = newShares > 0 ? (oldShares * oldCost + quantity * price + fee) / newShares : 0;
    } else {
        newShares = qMax(0.0, oldShares - quantity);
        realizedPnl = (price - oldCost) * quantity - fee;
        if (newShares <= 1e-8) {
            newShares = 0;
            newCost = 0;
        }
    }

    if (!db_.transaction()) {
        setStatus("无法启动交易保存事务");
        return false;
    }
    QSqlQuery insert(db_);
    const double amount = quantity * price;
    const double feeRate = amount > 0 ? fee / amount * 10000.0 : 0.0;
    insert.prepare("INSERT INTO dividend_trades (holding_id,market,symbol,name,trade_date,side,shares,price,fee,currency,instrument_type,fee_rate_per_ten_thousand,fee_is_actual,realized_pnl,note,created_at) "
                   "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    const QVariantList values{holdingId, market, symbol, name, tradeDate.toString("yyyy-MM-dd"), side,
                              quantity, price, fee, currency, instrumentType, feeRate,
                              row.value("fee_is_actual").toBool() ? 1 : 0, realizedPnl, text(row, "note"), nowIso()};
    for (const QVariant& value : values)
        insert.addBindValue(value);
    QSqlQuery update(db_);
    update.prepare("UPDATE dividend_holdings SET shares=?,cost_price=?,updated_at=? WHERE id=?");
    update.addBindValue(newShares); update.addBindValue(newCost); update.addBindValue(nowIso()); update.addBindValue(holdingId);
    if (!insert.exec() || !update.exec() || !db_.commit()) {
        db_.rollback();
        setStatus("交易保存失败：" + (insert.lastError().isValid() ? insert.lastError().text() : update.lastError().text()));
        return false;
    }
    setStatus(side + "已记录，持仓与成本价已更新");
    loadData();
    return true;
}

bool DividendController::updateTradeFee(int tradeId, double actualFee, const QString& note) {
    if (tradeId <= 0 || actualFee < 0) {
        setStatus("实际费用不能为负数");
        return false;
    }
    QSqlQuery trade(db_);
    trade.prepare("SELECT holding_id,side,shares,price,fee,realized_pnl FROM dividend_trades WHERE id=?");
    trade.addBindValue(tradeId);
    if (!trade.exec() || !trade.next()) {
        setStatus("找不到对应交易");
        return false;
    }
    const int holdingId = trade.value(0).toInt();
    const QString side = trade.value(1).toString();
    const double amount = trade.value(2).toDouble() * trade.value(3).toDouble();
    const double oldFee = trade.value(4).toDouble();
    const double delta = actualFee - oldFee;
    const double newRealizedPnl = trade.value(5).toDouble() - (side == "卖出" ? delta : 0.0);
    if (!db_.transaction()) {
        setStatus("无法启动费用修正事务");
        return false;
    }
    QSqlQuery updateTrade(db_);
    updateTrade.prepare("UPDATE dividend_trades SET fee=?,fee_rate_per_ten_thousand=?,fee_is_actual=1,realized_pnl=?,note=? WHERE id=?");
    updateTrade.addBindValue(actualFee);
    updateTrade.addBindValue(amount > 0 ? actualFee / amount * 10000.0 : 0.0);
    updateTrade.addBindValue(newRealizedPnl);
    updateTrade.addBindValue(note.trimmed());
    updateTrade.addBindValue(tradeId);
    bool ok = updateTrade.exec();
    if (ok && side == "买入" && qAbs(delta) > 1e-10) {
        QSqlQuery updateHolding(db_);
        updateHolding.prepare("UPDATE dividend_holdings SET cost_price=CASE WHEN shares>0 THEN MAX(0,cost_price+?/shares) ELSE cost_price END,updated_at=? WHERE id=? AND archived=0");
        updateHolding.addBindValue(delta);
        updateHolding.addBindValue(nowIso());
        updateHolding.addBindValue(holdingId);
        ok = updateHolding.exec();
    }
    if (!ok || !db_.commit()) {
        db_.rollback();
        setStatus("实际费用保存失败");
        return false;
    }
    setStatus("交割单实际费用已保存，相关成本或已实现盈亏已修正");
    loadData();
    return true;
}

void DividendController::loadMonth(const QString& month) {
    const QString normalized = normalizedMonth(month);
    if (activeMonth_ != normalized) { activeMonth_ = normalized; emit activeMonthChanged(); }
    loadData();
}

void DividendController::refresh() { loadData(); }

void DividendController::loadData() {
    applyDueDividendAdjustments();
    holdings_.clear(); events_.clear(); trades_.clear(); adjustments_.clear();
    QSqlQuery h(db_);
    if (h.exec("SELECT * FROM dividend_holdings WHERE archived=0 ORDER BY market,symbol"))
        while (h.next()) holdings_.append(queryRow(h));
    QSqlQuery e(db_);
    e.prepare("SELECT e.* FROM dividend_events e WHERE e.archived=0 "
              "AND EXISTS (SELECT 1 FROM dividend_holdings h WHERE h.archived=0 AND h.market=e.market AND h.symbol=e.symbol) "
              "AND (substr(e.record_date,1,7)=? OR substr(e.ex_date,1,7)=? OR substr(e.pay_date,1,7)=?) "
              "ORDER BY COALESCE(NULLIF(e.ex_date,''),NULLIF(e.record_date,''),e.pay_date),e.symbol");
    e.addBindValue(activeMonth_); e.addBindValue(activeMonth_); e.addBindValue(activeMonth_);
    if (e.exec()) while (e.next()) events_.append(queryRow(e));
    QSqlQuery trades(db_);
    if (trades.exec("SELECT * FROM dividend_trades ORDER BY trade_date DESC,id DESC LIMIT 300"))
        while (trades.next()) trades_.append(queryRow(trades));
    QSqlQuery adjustments(db_);
    if (adjustments.exec("SELECT * FROM dividend_adjustments ORDER BY ex_date DESC,id DESC LIMIT 300"))
        while (adjustments.next()) adjustments_.append(queryRow(adjustments));
    buildCalendar(); buildForecasts(); emit dataChanged();
}

void DividendController::applyDueDividendAdjustments() {
    QSqlQuery due(db_);
    due.prepare("SELECT e.id,h.id,h.market,h.symbol,h.name,e.ex_date,h.shares,e.amount_per_share,h.cost_price,h.currency "
                "FROM dividend_events e JOIN dividend_holdings h ON h.market=e.market AND h.symbol=e.symbol "
                "WHERE e.archived=0 AND h.archived=0 AND h.shares>0 AND e.amount_per_share>0 "
                "AND e.ex_date<>'' AND e.ex_date<=? AND e.ex_date>=h.auto_adjust_from "
                "AND NOT EXISTS (SELECT 1 FROM dividend_adjustments a WHERE a.event_id=e.id AND a.holding_id=h.id) "
                "ORDER BY e.ex_date,e.id");
    due.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    if (!due.exec()) {
        setStatus("检查除息复权失败：" + due.lastError().text());
        return;
    }
    QList<QVariantList> rows;
    while (due.next()) {
        QVariantList row;
        for (int i = 0; i < 10; ++i) row.append(due.value(i));
        rows.append(row);
    }
    if (rows.isEmpty())
        return;
    if (!db_.transaction()) {
        setStatus("无法启动除息复权事务");
        return;
    }
    int applied = 0;
    for (const QVariantList& row : rows) {
        const int eventId = row[0].toInt();
        const int holdingId = row[1].toInt();
        const double shares = row[6].toDouble();
        const double amount = row[7].toDouble();
        const double oldCost = row[8].toDouble();
        const double newCost = qMax(0.0, oldCost - amount);
        QSqlQuery insert(db_);
        insert.prepare("INSERT OR IGNORE INTO dividend_adjustments (event_id,holding_id,market,symbol,name,ex_date,shares,"
                       "amount_per_share,gross_amount,old_cost_price,new_cost_price,currency,created_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)");
        const QVariantList values{eventId, holdingId, row[2], row[3], row[4], row[5], shares, amount,
                                  shares * amount, oldCost, newCost, row[9], nowIso()};
        for (const QVariant& item : values) insert.addBindValue(item);
        if (!insert.exec()) { db_.rollback(); setStatus("保存除息复权记录失败：" + insert.lastError().text()); return; }
        if (insert.numRowsAffected() > 0) {
            QSqlQuery update(db_);
            update.prepare("UPDATE dividend_holdings SET cost_price=?,updated_at=? WHERE id=?");
            update.addBindValue(newCost); update.addBindValue(nowIso()); update.addBindValue(holdingId);
            if (!update.exec()) { db_.rollback(); setStatus("更新除息后成本失败：" + update.lastError().text()); return; }
            ++applied;
        }
    }
    if (!db_.commit()) {
        db_.rollback(); setStatus("提交除息复权失败"); return;
    }
    if (applied > 0)
        setStatus(QString("已自动完成 %1 笔除息成本调整（税前毛额）").arg(applied));
}

void DividendController::buildCalendar() {
    calendarDays_.clear();
    const QDate first = QDate::fromString(activeMonth_ + "-01", "yyyy-MM-dd");
    const QDate start = first.addDays(1 - first.dayOfWeek());
    QDate date = start;
    while (calendarDays_.size() < 30) {
        if (date.dayOfWeek() > 5) {
            date = date.addDays(1);
            continue;
        }
        const QString iso = date.toString("yyyy-MM-dd");
        QVariantList dayEvents;
        for (const QVariant& value : events_) {
            const QVariantMap event = value.toMap();
            auto append = [&](const QString& field, const QString& kind) {
                if (event.value(field).toString() == iso) { QVariantMap item = event; item["date_kind"] = kind; dayEvents.append(item); }
            };
            append("record_date", "登记"); append("ex_date", "除权"); append("pay_date", "派息");
        }
        calendarDays_.append(QVariantMap{{"date", iso}, {"day", date.day()},
                                         {"inMonth", date.month() == first.month()},
                                         {"today", date == QDate::currentDate()}, {"events", dayEvents}});
        date = date.addDays(1);
    }
}

void DividendController::buildForecasts() {
    forecasts_.clear();
    const int currentYear = QDate::currentDate().year();
    for (const QVariant& value : holdings_) {
        QVariantMap holding = value.toMap();
        QMap<int, double> totals;
        QSqlQuery q(db_);
        q.prepare("SELECT ex_date,pay_date,amount_per_share FROM dividend_events WHERE archived=0 AND market=? AND symbol=? AND amount_per_share>0");
        q.addBindValue(holding.value("market")); q.addBindValue(holding.value("symbol"));
        if (q.exec()) while (q.next()) {
            const QString dateText = q.value(0).toString().isEmpty() ? q.value(1).toString() : q.value(0).toString();
            const int year = QDate::fromString(dateText, "yyyy-MM-dd").year();
            if (year > 0 && year <= currentYear) totals[year] += q.value(2).toDouble();
        }
        auto average = [&](int years) {
            if (totals.isEmpty())
                return 0.0;
            const int firstObservedYear = totals.firstKey();
            const int startYear = qMax(currentYear - years, firstObservedYear);
            if (startYear > currentYear - 1)
                return 0.0;
            double sum = 0;
            for (int year = startYear; year <= currentYear - 1; ++year)
                sum += totals.value(year, 0.0);
            return sum / (currentYear - startYear);
        };
        const double one = average(1), three = average(3), five = average(5);
        const int observed = totals.size();
        holding["average1y"] = one * holding.value("shares").toDouble();
        holding["average3y"] = three * holding.value("shares").toDouble();
        holding["average5y"] = five * holding.value("shares").toDouble();
        holding["estimatedAnnual"] = one * holding.value("shares").toDouble();
        holding["confidence"] = observed >= 5 ? "较高" : observed >= 3 ? "中等" : observed > 0 ? "较低" : "无历史";
        forecasts_.append(holding);
    }
}

void DividendController::refreshOnline() {
    if (refreshing_ || holdings_.isEmpty()) { setStatus(holdings_.isEmpty() ? "请先添加持仓" : "正在更新"); return; }
    QJsonArray rows; for (const QVariant& value : holdings_) rows.append(QJsonObject::fromVariantMap(value.toMap()));
    const QString input = AppPaths::cache() + "/dividend_holdings.json";
    fetchOutputPath_ = AppPaths::cache() + "/dividend_events.json";
    QSaveFile file(input);
    if (!file.open(QIODevice::WriteOnly)) { setStatus("无法写入更新任务"); return; }
    file.write(QJsonDocument(rows).toJson()); if (!file.commit()) { setStatus("无法保存更新任务"); return; }
    process_ = new QProcess(this); setRefreshing(true); setStatus("正在补齐 A/H/美股股息历史…");
    connect(process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        const QString error = QString::fromUtf8(process_->readAllStandardError()).trimmed();
        if (code == 0) importFetchedEvents(fetchOutputPath_);
        else setStatus("股息数据更新失败" + (error.isEmpty() ? QString() : "：" + error.left(180)));
        process_->deleteLater(); process_ = nullptr; setRefreshing(false);
    });
    process_->start(settings_->pythonPath(), {AppPaths::resourceFile("scripts/dividend_fetch.py"), "--holdings", input, "--output", fetchOutputPath_});
}

void DividendController::importFetchedEvents(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { setStatus("更新完成，但无法读取结果"); return; }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonArray rows = root.value("events").toArray();
    if (!db_.transaction()) { setStatus("无法启动导入事务"); return; }
    for (const QJsonValue& value : rows) {
        const QVariantMap row = value.toObject().toVariantMap(); QSqlQuery q(db_);
        q.prepare("INSERT OR IGNORE INTO dividend_events (market,symbol,name,declaration_date,record_date,ex_date,pay_date,"
                  "amount_per_share,currency,event_type,source,announced,notes,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        const QStringList keys{"market","symbol","name","declaration_date","record_date","ex_date","pay_date","amount_per_share","currency","event_type","source","announced","notes"};
        for (const QString& key : keys) q.addBindValue(row.value(key));
        q.addBindValue(nowIso()); q.addBindValue(nowIso());
        if (!q.exec()) { db_.rollback(); setStatus("导入股息数据失败：" + q.lastError().text()); return; }
    }
    db_.commit(); setStatus(QString("已合并 %1 条事件；来源失败 %2 项").arg(rows.size()).arg(root.value("errors").toObject().size())); loadData();
}
