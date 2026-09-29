#include "NotesController.h"

#include "AppPaths.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextStream>

namespace {
constexpr int kReviewSchemaVersion = 2;

QString nowIso() { return QDateTime::currentDateTime().toString(Qt::ISODate); }
QString today() { return QDate::currentDate().toString("yyyy-MM-dd"); }
QString currentMonth() { return QDate::currentDate().toString("yyyy-MM"); }

QString normalizeMonth(QString value) {
    value = value.trimmed();
    if (value.size() >= 7)
        value = value.left(7);
    if (QDate::fromString(value + "-01", "yyyy-MM-dd").isValid())
        return value;
    return currentMonth();
}

QString normalizeDate(QString value) {
    value = value.trimmed();
    if (QDate::fromString(value, "yyyy-MM-dd").isValid())
        return value;
    if (QDate::fromString(value, "yyyyMMdd").isValid())
        return QDate::fromString(value, "yyyyMMdd").toString("yyyy-MM-dd");
    return today();
}

QString defaultWeekKey(const QString& dateText) {
    const QDate date = QDate::fromString(normalizeDate(dateText), "yyyy-MM-dd");
    int year = date.year();
    const int week = date.weekNumber(&year);
    return QString("%1-W%2").arg(year).arg(week, 2, 10, QChar('0'));
}

QDate weekStartDate(const QString& weekKey) {
    const QRegularExpression re(QStringLiteral("^(\\d{4})-W(\\d{1,2})$"));
    const QRegularExpressionMatch match = re.match(weekKey.trimmed());
    if (!match.hasMatch())
        return {};
    const int year = match.captured(1).toInt();
    const int week = match.captured(2).toInt();
    if (week < 1 || week > 53)
        return {};
    const QDate jan4(year, 1, 4);
    return jan4.addDays(1 - jan4.dayOfWeek()).addDays((week - 1) * 7);
}

bool weekTouchesMonth(const QString& weekKey, const QString& month) {
    const QDate monday = weekStartDate(weekKey);
    if (!monday.isValid())
        return false;
    for (int i = 0; i < 7; ++i) {
        if (monday.addDays(i).toString("yyyy-MM") == month)
            return true;
    }
    return false;
}

QString weekSortDateInMonth(const QString& weekKey, const QString& month) {
    const QDate monday = weekStartDate(weekKey);
    if (!monday.isValid())
        return month + "-31";
    QDate last;
    for (int i = 0; i < 7; ++i) {
        const QDate day = monday.addDays(i);
        if (day.toString("yyyy-MM") == month)
            last = day;
    }
    return last.isValid() ? last.toString("yyyy-MM-dd") : month + "-31";
}

int wordCount(const QString& text) {
    return text.simplified().isEmpty() ? 0 : text.simplified().split(' ').size();
}

QString slug(QString title) {
    QString out = title.trimmed().toLower();
    out.replace(QRegularExpression("[^a-z0-9\\u4e00-\\u9fff]+"), "-");
    out = out.replace(QRegularExpression("^-+|-+$"), "");
    return out.isEmpty() ? "review" : out.left(80);
}

QString str(const QVariantMap& row, const QString& key, const QString& fallback = QString()) {
    const QString value = row.value(key).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

double numberOrZero(const QVariantMap& row, const QString& key) {
    QString text = row.value(key).toString().trimmed();
    const bool percent = text.contains('%');
    text.remove('%');
    text.remove(',');
    text.replace(QStringLiteral("－"), QStringLiteral("-"));
    text.replace(QStringLiteral("＋"), QStringLiteral("+"));
    bool ok = false;
    const double v = text.toDouble(&ok);
    if (!ok)
        return 0.0;
    return percent ? v / 100.0 : v;
}

QVariant boolInt(const QVariantMap& row, const QString& key) {
    return row.value(key).toBool() ? 1 : 0;
}
}

NotesController::NotesController(QObject* parent) : QObject(parent), activeMonth_(currentMonth()) {
    AppPaths::ensureAll();
    ensureDatabase();
    refresh();
    loadMonth(activeMonth_);
}

bool NotesController::execSql(const QString& sql) {
    QSqlQuery q(db_);
    if (q.exec(sql))
        return true;
    setStatus("笔记数据库迁移失败：" + q.lastError().text());
    return false;
}

bool NotesController::ensureDatabase() {
    if (db_.isValid() && db_.isOpen())
        return true;

    const QString connectionName = "myquant_notes";
    if (QSqlDatabase::contains(connectionName))
        db_ = QSqlDatabase::database(connectionName);
    else
        db_ = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db_.setDatabaseName(AppPaths::data() + "/myquant.db");
    if (!db_.open()) {
        setStatus("笔记数据库打开失败：" + db_.lastError().text());
        return false;
    }

    if (!execSql("CREATE TABLE IF NOT EXISTS notes ("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                 "title TEXT NOT NULL,"
                 "content TEXT NOT NULL,"
                 "category TEXT NOT NULL,"
                 "tags TEXT,"
                 "tickers TEXT,"
                 "sentiment TEXT,"
                 "direction TEXT,"
                 "review_date TEXT,"
                 "favorite INTEGER DEFAULT 0,"
                 "archived INTEGER DEFAULT 0,"
                 "created_at TEXT NOT NULL,"
                 "updated_at TEXT NOT NULL,"
                 "word_count INTEGER DEFAULT 0"
                 ")"))
        return false;

    return ensureReviewSchema();
}

int NotesController::schemaVersion() const {
    QSqlQuery q(db_);
    if (!q.exec("SELECT value FROM schema_meta WHERE key='notes_schema_version'") || !q.next())
        return 1;
    return q.value(0).toInt();
}

bool NotesController::setSchemaVersion(int version) {
    QSqlQuery q(db_);
    q.prepare("INSERT OR REPLACE INTO schema_meta (key, value, updated_at) VALUES ('notes_schema_version', ?, ?)");
    q.addBindValue(version);
    q.addBindValue(nowIso());
    return q.exec();
}

bool NotesController::backupDatabaseForMigration(int fromVersion) {
    const QString dbPath = AppPaths::data() + "/myquant.db";
    if (!QFileInfo::exists(dbPath))
        return true;
    const QString backupDir = AppPaths::root() + "/backups";
    QDir().mkpath(backupDir);
    QString backupPath = backupDir + QString("/myquant-before-notes-v%1.db").arg(fromVersion);
    if (QFileInfo::exists(backupPath))
        return true;
    db_.commit();
    db_.close();
    const bool ok = QFile::copy(dbPath, backupPath);
    db_.open();
    if (!ok) {
        setStatus("迁移前备份数据库失败：" + backupPath);
        return false;
    }
    return true;
}

bool NotesController::ensureReviewSchema() {
    if (!execSql("CREATE TABLE IF NOT EXISTS schema_meta ("
                 "key TEXT PRIMARY KEY,"
                 "value INTEGER NOT NULL,"
                 "updated_at TEXT NOT NULL"
                 ")"))
        return false;

    const int version = schemaVersion();
    if (version < kReviewSchemaVersion && !backupDatabaseForMigration(version))
        return false;

    if (!db_.transaction()) {
        setStatus("笔记数据库事务启动失败：" + db_.lastError().text());
        return false;
    }

    const QStringList ddl = {
        "CREATE TABLE IF NOT EXISTS review_market_days ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "review_date TEXT NOT NULL,"
        "market_status TEXT,"
        "index_change TEXT,"
        "volume_change TEXT,"
        "market_theme TEXT,"
        "strong_sectors TEXT,"
        "weak_sectors TEXT,"
        "news TEXT,"
        "emotion TEXT,"
        "market_view TEXT,"
        "archived INTEGER DEFAULT 0,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_review_market_days_month ON review_market_days(review_date, archived)",
        "CREATE TABLE IF NOT EXISTS review_watchlist_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "review_date TEXT NOT NULL,"
        "symbol TEXT,"
        "name TEXT,"
        "industry_theme TEXT,"
        "current_price REAL,"
        "technical_position TEXT,"
        "fundamental_event TEXT,"
        "valuation_status TEXT,"
        "watch_reason TEXT,"
        "trigger_condition TEXT,"
        "invalidation_condition TEXT,"
        "priority TEXT,"
        "archived INTEGER DEFAULT 0,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_review_watchlist_month ON review_watchlist_items(review_date, archived)",
        "CREATE TABLE IF NOT EXISTS review_trade_executions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "trade_date TEXT NOT NULL,"
        "symbol TEXT,"
        "name TEXT,"
        "action TEXT,"
        "price REAL,"
        "position_pct REAL,"
        "trade_reasons TEXT,"
        "target_price REAL,"
        "stop_loss REAL,"
        "holding_period TEXT,"
        "pre_trade_emotion TEXT,"
        "followed_plan INTEGER DEFAULT 1,"
        "sell_reason TEXT,"
        "actual_return REAL,"
        "error_attribution TEXT,"
        "next_improvement TEXT,"
        "archived INTEGER DEFAULT 0,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_review_trades_month ON review_trade_executions(trade_date, archived)",
        "CREATE TABLE IF NOT EXISTS review_period_summaries ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "period_type TEXT NOT NULL,"
        "period_key TEXT NOT NULL,"
        "total_return REAL,"
        "benchmark_return REAL,"
        "excess_return REAL,"
        "win_rate REAL,"
        "avg_win REAL,"
        "avg_loss REAL,"
        "payoff_ratio REAL,"
        "max_drawdown REAL,"
        "worst_trade TEXT,"
        "best_trade TEXT,"
        "common_mistake TEXT,"
        "next_rule TEXT,"
        "notes TEXT,"
        "archived INTEGER DEFAULT 0,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_review_summaries_month ON review_period_summaries(period_key, period_type, archived)",
    };

    for (const QString& sql : ddl) {
        QSqlQuery q(db_);
        if (!q.exec(sql)) {
            db_.rollback();
            setStatus("笔记数据库迁移失败：" + q.lastError().text());
            return false;
        }
    }
    if (!setSchemaVersion(kReviewSchemaVersion)) {
        db_.rollback();
        setStatus("笔记数据库版本写入失败");
        return false;
    }
    if (!db_.commit()) {
        setStatus("笔记数据库迁移提交失败：" + db_.lastError().text());
        return false;
    }
    return true;
}

void NotesController::setStatus(const QString& message) {
    if (statusMessage_ == message)
        return;
    statusMessage_ = message;
    emit statusMessageChanged();
}

QVariantMap NotesController::rowToMap(const QSqlQuery& q) const {
    QVariantMap row;
    const QSqlRecord rec = q.record();
    for (int i = 0; i < rec.count(); ++i)
        row.insert(rec.fieldName(i), q.value(i));
    row["archived"] = row.value("archived").toBool();
    return row;
}

QVariantMap NotesController::rowToNote(const QSqlQuery& q) const {
    return {
        {"id", q.value("id").toInt()},
        {"title", q.value("title").toString()},
        {"content", q.value("content").toString()},
        {"category", q.value("category").toString()},
        {"tags", q.value("tags").toString()},
        {"tickers", q.value("tickers").toString()},
        {"sentiment", q.value("sentiment").toString()},
        {"direction", q.value("direction").toString()},
        {"reviewDate", q.value("review_date").toString()},
        {"favorite", q.value("favorite").toBool()},
        {"archived", q.value("archived").toBool()},
        {"createdAt", q.value("created_at").toString()},
        {"updatedAt", q.value("updated_at").toString()},
        {"wordCount", q.value("word_count").toInt()},
    };
}

void NotesController::refresh(const QString& query) {
    if (!ensureDatabase())
        return;
    notes_.clear();

    QSqlQuery q(db_);
    if (query.trimmed().isEmpty()) {
        q.prepare("SELECT * FROM notes WHERE archived = 0 ORDER BY favorite DESC, updated_at DESC");
    } else {
        q.prepare("SELECT * FROM notes WHERE archived = 0 AND "
                  "(title LIKE ? OR content LIKE ? OR tags LIKE ? OR tickers LIKE ?) "
                  "ORDER BY favorite DESC, updated_at DESC");
        const QString like = "%" + query.trimmed() + "%";
        q.addBindValue(like);
        q.addBindValue(like);
        q.addBindValue(like);
        q.addBindValue(like);
    }
    if (!q.exec()) {
        setStatus("读取旧笔记失败：" + q.lastError().text());
        return;
    }
    while (q.next())
        notes_.append(rowToNote(q));
    emit notesChanged();
}

void NotesController::loadMonth(const QString& yearMonth) {
    if (!ensureDatabase())
        return;
    const QString month = normalizeMonth(yearMonth);
    if (activeMonth_ != month) {
        activeMonth_ = month;
        emit activeMonthChanged();
    }
    loadReviewTables();
}

void NotesController::ensureMonth(const QString& yearMonth) {
    loadMonth(yearMonth);
    setStatus(activeMonth_ + " 月份已就绪；不会自动创建空白日表");
}

QVariantList NotesController::buildTimelineCards() const {
    struct Card {
        QString key;
        QString type;
        QString label;
        QString date;
        QString periodKey;
        int count = 0;
    };
    QMap<QString, Card> cards;
    auto dayLabel = [](const QString& dateText) {
        const QDate date = QDate::fromString(dateText, "yyyy-MM-dd");
        if (!date.isValid())
            return dateText;
        return QString("%1.%2").arg(date.month()).arg(date.day());
    };
    auto addDay = [&](const QString& dateText) {
        const QString date = normalizeDate(dateText);
        if (!date.startsWith(activeMonth_))
            return;
        Card card = cards.value("D:" + date);
        card.key = "D:" + date;
        card.type = "day";
        card.label = dayLabel(date);
        card.date = date;
        card.periodKey = date;
        card.count += 1;
        cards.insert(card.key, card);
    };
    for (const QVariant& value : marketDays_)
        addDay(value.toMap().value("review_date").toString());
    for (const QVariant& value : watchlistItems_)
        addDay(value.toMap().value("review_date").toString());
    for (const QVariant& value : tradeExecutions_)
        addDay(value.toMap().value("trade_date").toString());

    for (const QVariant& value : periodSummaries_) {
        const QVariantMap row = value.toMap();
        const QString type = row.value("period_type").toString();
        const QString periodKey = row.value("period_key").toString();
        if (type == "week" && weekTouchesMonth(periodKey, activeMonth_)) {
            Card card;
            card.key = "W:" + periodKey;
            card.type = "week";
            card.label = "周总结";
            card.periodKey = periodKey;
            card.date = weekSortDateInMonth(periodKey, activeMonth_);
            card.count = 1;
            cards.insert(card.key, card);
        } else if (type == "month" && periodKey == activeMonth_) {
            Card card;
            card.key = "M:" + activeMonth_;
            card.type = "month";
            card.label = "月总结";
            card.periodKey = activeMonth_;
            card.date = activeMonth_ + "-99";
            card.count = 1;
            cards.insert(card.key, card);
        }
    }

    QVariantList out;
    for (const Card& card : cards) {
        out.append(QVariantMap{{"key", card.key},
                               {"type", card.type},
                               {"label", card.label},
                               {"date", card.date},
                               {"periodKey", card.periodKey},
                               {"count", card.count}});
    }
    std::sort(out.begin(), out.end(), [](const QVariant& a, const QVariant& b) {
        const QVariantMap aa = a.toMap();
        const QVariantMap bb = b.toMap();
        if (aa.value("date").toString() != bb.value("date").toString())
            return aa.value("date").toString() < bb.value("date").toString();
        return aa.value("type").toString() < bb.value("type").toString();
    });
    return out;
}

void NotesController::loadReviewTables() {
    const QString like = activeMonth_ + "%";
    auto load = [&](const QString& sql, QVariantList& out) {
        out.clear();
        QSqlQuery q(db_);
        q.prepare(sql);
        q.addBindValue(like);
        if (!q.exec()) {
            setStatus("读取复盘数据失败：" + q.lastError().text());
            return;
        }
        while (q.next())
            out.append(rowToMap(q));
    };
    load("SELECT * FROM review_market_days WHERE archived=0 AND review_date LIKE ? ORDER BY review_date DESC, id DESC",
         marketDays_);
    load("SELECT * FROM review_watchlist_items WHERE archived=0 AND review_date LIKE ? ORDER BY review_date DESC, priority, id DESC",
         watchlistItems_);
    load("SELECT * FROM review_trade_executions WHERE archived=0 AND trade_date LIKE ? ORDER BY trade_date DESC, id DESC",
         tradeExecutions_);
    periodSummaries_.clear();
    QSqlQuery summaries(db_);
    summaries.prepare("SELECT * FROM review_period_summaries WHERE archived=0 "
                      "AND (period_key=? OR period_key LIKE ?) "
                      "ORDER BY period_type, period_key DESC");
    summaries.addBindValue(activeMonth_);
    summaries.addBindValue(activeMonth_.left(4) + "-W%");
    if (!summaries.exec()) {
        setStatus("读取复盘总结失败：" + summaries.lastError().text());
        return;
    }
    while (summaries.next()) {
        QVariantMap row = rowToMap(summaries);
        const QString type = row.value("period_type").toString();
        const QString periodKey = row.value("period_key").toString();
        if ((type == "month" && periodKey == activeMonth_) || (type == "week" && weekTouchesMonth(periodKey, activeMonth_)))
            periodSummaries_.append(row);
    }
    timelineCards_ = buildTimelineCards();
    setStatus(QString("%1 复盘库已载入：市场 %2，观察 %3，交易 %4，总结 %5")
                  .arg(activeMonth_)
                  .arg(marketDays_.size())
                  .arg(watchlistItems_.size())
                  .arg(tradeExecutions_.size())
                  .arg(periodSummaries_.size()));
    emit reviewDataChanged();
}

QVariantMap NotesController::createMarketDay(const QString& date) const {
    return {{"id", 0},
            {"review_date", normalizeDate(date)},
            {"market_status", "震荡"},
            {"index_change", ""},
            {"volume_change", ""},
            {"market_theme", ""},
            {"strong_sectors", ""},
            {"weak_sectors", ""},
            {"news", ""},
            {"emotion", "中性"},
            {"market_view", ""},
            {"archived", false}};
}

QVariantMap NotesController::createWatchlistItem(const QString& date) const {
    return {{"id", 0},
            {"review_date", normalizeDate(date)},
            {"symbol", ""},
            {"name", ""},
            {"industry_theme", ""},
            {"current_price", 0.0},
            {"technical_position", ""},
            {"fundamental_event", ""},
            {"valuation_status", ""},
            {"watch_reason", ""},
            {"trigger_condition", ""},
            {"invalidation_condition", ""},
            {"priority", "B"},
            {"archived", false}};
}

QVariantMap NotesController::createTradeExecution(const QString& date) const {
    return {{"id", 0},
            {"trade_date", normalizeDate(date)},
            {"symbol", ""},
            {"name", ""},
            {"action", "买入"},
            {"price", 0.0},
            {"position_pct", 0.0},
            {"trade_reasons", ""},
            {"target_price", 0.0},
            {"stop_loss", 0.0},
            {"holding_period", "几天"},
            {"pre_trade_emotion", "冷静"},
            {"followed_plan", true},
            {"sell_reason", ""},
            {"actual_return", 0.0},
            {"error_attribution", "待复盘"},
            {"next_improvement", ""},
            {"archived", false}};
}

QVariantMap NotesController::createPeriodSummary(const QString& periodType, const QString& periodKey) const {
    const QString type = periodType == "month" ? "month" : "week";
    const QString key = periodKey.trimmed().isEmpty()
                            ? (type == "month" ? activeMonth_ : defaultWeekKey(today()))
                            : periodKey.trimmed();
    return {{"id", 0},
            {"period_type", type},
            {"period_key", key},
            {"total_return", 0.0},
            {"benchmark_return", 0.0},
            {"excess_return", 0.0},
            {"win_rate", 0.0},
            {"avg_win", 0.0},
            {"avg_loss", 0.0},
            {"payoff_ratio", 0.0},
            {"max_drawdown", 0.0},
            {"worst_trade", ""},
            {"best_trade", ""},
            {"common_mistake", ""},
            {"next_rule", ""},
            {"notes", ""},
            {"archived", false}};
}

bool NotesController::saveRow(const QString& table, const QStringList& fields, QVariantMap row, const QString& label) {
    if (!ensureDatabase())
        return false;
    const int id = row.value("id").toInt();
    const QString ts = nowIso();
    if (!db_.transaction()) {
        setStatus(label + "保存失败：事务启动失败");
        return false;
    }

    QSqlQuery q(db_);
    if (id > 0) {
        QStringList sets;
        for (const QString& field : fields)
            sets.append(field + "=?");
        sets.append("updated_at=?");
        q.prepare(QString("UPDATE %1 SET %2 WHERE id=?").arg(table, sets.join(',')));
        for (const QString& field : fields)
            q.addBindValue(row.value(field));
        q.addBindValue(ts);
        q.addBindValue(id);
    } else {
        QStringList allFields = fields;
        allFields << "archived"
                  << "created_at"
                  << "updated_at";
        QStringList marks;
        for (int i = 0; i < allFields.size(); ++i)
            marks.append("?");
        q.prepare(QString("INSERT INTO %1 (%2) VALUES (%3)").arg(table, allFields.join(','), marks.join(',')));
        for (const QString& field : fields)
            q.addBindValue(row.value(field));
        q.addBindValue(boolInt(row, "archived"));
        q.addBindValue(ts);
        q.addBindValue(ts);
    }

    if (!q.exec()) {
        db_.rollback();
        setStatus(label + "保存失败：" + q.lastError().text());
        return false;
    }
    if (!db_.commit()) {
        setStatus(label + "保存失败：" + db_.lastError().text());
        return false;
    }
    loadReviewTables();
    setStatus(label + "已保存");
    return true;
}

bool NotesController::saveMarketDay(const QVariantMap& row) {
    QVariantMap r = row;
    r["review_date"] = normalizeDate(str(row, "review_date"));
    return saveRow("review_market_days",
                   {"review_date", "market_status", "index_change", "volume_change", "market_theme",
                    "strong_sectors", "weak_sectors", "news", "emotion", "market_view"},
                   r, "每日市场");
}

bool NotesController::saveWatchlistItem(const QVariantMap& row) {
    QVariantMap r = row;
    r["review_date"] = normalizeDate(str(row, "review_date"));
    r["current_price"] = numberOrZero(row, "current_price");
    return saveRow("review_watchlist_items",
                   {"review_date", "symbol", "name", "industry_theme", "current_price", "technical_position",
                    "fundamental_event", "valuation_status", "watch_reason", "trigger_condition",
                    "invalidation_condition", "priority"},
                   r, "观察项");
}

bool NotesController::saveTradeExecution(const QVariantMap& row) {
    QVariantMap r = row;
    r["trade_date"] = normalizeDate(str(row, "trade_date"));
    r["price"] = numberOrZero(row, "price");
    r["position_pct"] = numberOrZero(row, "position_pct");
    r["target_price"] = numberOrZero(row, "target_price");
    r["stop_loss"] = numberOrZero(row, "stop_loss");
    r["actual_return"] = numberOrZero(row, "actual_return");
    r["followed_plan"] = row.value("followed_plan").toBool() ? 1 : 0;
    return saveRow("review_trade_executions",
                   {"trade_date", "symbol", "name", "action", "price", "position_pct", "trade_reasons",
                    "target_price", "stop_loss", "holding_period", "pre_trade_emotion", "followed_plan",
                    "sell_reason", "actual_return", "error_attribution", "next_improvement"},
                   r, "交易执行");
}

bool NotesController::savePeriodSummary(const QVariantMap& row) {
    QVariantMap r = row;
    const QString type = str(row, "period_type", "week") == "month" ? "month" : "week";
    r["period_type"] = type;
    r["period_key"] = str(row, "period_key", type == "month" ? activeMonth_ : defaultWeekKey(today()));
    for (const QString& key : {"total_return", "benchmark_return", "excess_return", "win_rate", "avg_win",
                               "avg_loss", "payoff_ratio", "max_drawdown"})
        r[key] = numberOrZero(row, key);
    return saveRow("review_period_summaries",
                   {"period_type", "period_key", "total_return", "benchmark_return", "excess_return",
                    "win_rate", "avg_win", "avg_loss", "payoff_ratio", "max_drawdown", "worst_trade",
                    "best_trade", "common_mistake", "next_rule", "notes"},
                   r, type == "month" ? "月总结" : "周总结");
}

bool NotesController::archiveRow(const QString& table, int id, const QString& label) {
    if (!ensureDatabase() || id <= 0)
        return false;
    QSqlQuery q(db_);
    q.prepare(QString("UPDATE %1 SET archived=1, updated_at=? WHERE id=?").arg(table));
    q.addBindValue(nowIso());
    q.addBindValue(id);
    if (!q.exec()) {
        setStatus(label + "归档失败：" + q.lastError().text());
        return false;
    }
    loadReviewTables();
    setStatus(label + "已归档");
    return true;
}

bool NotesController::archiveMarketDay(int id) { return archiveRow("review_market_days", id, "每日市场"); }
bool NotesController::archiveWatchlistItem(int id) { return archiveRow("review_watchlist_items", id, "观察项"); }
bool NotesController::archiveTradeExecution(int id) { return archiveRow("review_trade_executions", id, "交易执行"); }
bool NotesController::archivePeriodSummary(int id) { return archiveRow("review_period_summaries", id, "周期总结"); }

QVariantMap NotesController::createFromTemplate(const QString& templateName) {
    const QString name = templateName.trimmed().isEmpty() ? "盘后复盘" : templateName.trimmed();
    QString content;
    QString category = name;
    if (name == "盘前计划") {
        content = "## 今日关注\n\n- \n\n## 交易计划\n\n- \n\n## 风险边界\n\n- \n";
    } else if (name == "盘中观察") {
        content = "## 盘面变化\n\n- \n\n## 异常信号\n\n- \n\n## 待验证\n\n- \n";
    } else if (name == "交易复盘") {
        content = "## 交易背景\n\n- \n\n## 执行\n\n- \n\n## 结果\n\n- \n\n## 下次改进\n\n- \n";
    } else if (name == "投研假设") {
        content = "## 假设\n\n- \n\n## 证据\n\n- \n\n## 反证\n\n- \n\n## 跟踪指标\n\n- \n";
    } else {
        content = "## 市场状态\n\n- \n\n## 组合变化\n\n- \n\n## 做对了什么\n\n- \n\n## 需要修正\n\n- \n";
    }

    return {{"id", 0},
            {"title", name + " " + today()},
            {"content", content},
            {"category", category},
            {"tags", ""},
            {"tickers", ""},
            {"sentiment", "中性"},
            {"direction", "观察"},
            {"reviewDate", today()},
            {"favorite", false},
            {"archived", false},
            {"createdAt", nowIso()},
            {"updatedAt", nowIso()},
            {"wordCount", wordCount(content)}};
}

bool NotesController::saveNote(const QVariantMap& note) {
    if (!ensureDatabase())
        return false;
    const int id = note.value("id").toInt();
    const QString title = note.value("title").toString().trimmed().isEmpty()
                              ? "未命名复盘"
                              : note.value("title").toString().trimmed();
    const QString content = note.value("content").toString();
    QSqlQuery q(db_);
    if (id > 0) {
        q.prepare("UPDATE notes SET title=?, content=?, category=?, tags=?, tickers=?, sentiment=?, "
                  "direction=?, review_date=?, favorite=?, archived=?, updated_at=?, word_count=? WHERE id=?");
        q.addBindValue(title);
        q.addBindValue(content);
        q.addBindValue(note.value("category").toString());
        q.addBindValue(note.value("tags").toString());
        q.addBindValue(note.value("tickers").toString());
        q.addBindValue(note.value("sentiment").toString());
        q.addBindValue(note.value("direction").toString());
        q.addBindValue(note.value("reviewDate").toString());
        q.addBindValue(note.value("favorite").toBool() ? 1 : 0);
        q.addBindValue(note.value("archived").toBool() ? 1 : 0);
        q.addBindValue(nowIso());
        q.addBindValue(wordCount(content));
        q.addBindValue(id);
    } else {
        q.prepare("INSERT INTO notes (title, content, category, tags, tickers, sentiment, direction, "
                  "review_date, favorite, archived, created_at, updated_at, word_count) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        q.addBindValue(title);
        q.addBindValue(content);
        q.addBindValue(note.value("category").toString());
        q.addBindValue(note.value("tags").toString());
        q.addBindValue(note.value("tickers").toString());
        q.addBindValue(note.value("sentiment").toString());
        q.addBindValue(note.value("direction").toString());
        q.addBindValue(note.value("reviewDate").toString().isEmpty() ? today() : note.value("reviewDate").toString());
        q.addBindValue(note.value("favorite").toBool() ? 1 : 0);
        q.addBindValue(note.value("archived").toBool() ? 1 : 0);
        q.addBindValue(nowIso());
        q.addBindValue(nowIso());
        q.addBindValue(wordCount(content));
    }
    if (!q.exec()) {
        setStatus("保存旧笔记失败：" + q.lastError().text());
        return false;
    }
    refresh();
    setStatus("旧笔记已保存");
    return true;
}

bool NotesController::deleteNote(int id) {
    if (!ensureDatabase() || id <= 0)
        return false;
    QSqlQuery q(db_);
    q.prepare("UPDATE notes SET archived=1, updated_at=? WHERE id=?");
    q.addBindValue(nowIso());
    q.addBindValue(id);
    if (!q.exec()) {
        setStatus("归档旧笔记失败：" + q.lastError().text());
        return false;
    }
    refresh();
    setStatus("旧笔记已归档");
    return true;
}

bool NotesController::toggleFavorite(int id) {
    if (!ensureDatabase() || id <= 0)
        return false;
    QSqlQuery q(db_);
    q.prepare("UPDATE notes SET favorite = CASE favorite WHEN 0 THEN 1 ELSE 0 END, updated_at=? WHERE id=?");
    q.addBindValue(nowIso());
    q.addBindValue(id);
    const bool ok = q.exec();
    refresh();
    return ok;
}

bool NotesController::toggleArchive(int id) {
    if (!ensureDatabase() || id <= 0)
        return false;
    QSqlQuery q(db_);
    q.prepare("UPDATE notes SET archived = CASE archived WHEN 0 THEN 1 ELSE 0 END, updated_at=? WHERE id=?");
    q.addBindValue(nowIso());
    q.addBindValue(id);
    const bool ok = q.exec();
    refresh();
    return ok;
}

QString NotesController::noteMarkdown(const QVariantMap& note) const {
    QString out;
    QTextStream s(&out);
    s << "# " << note.value("title").toString() << "\n\n";
    s << "- Category: " << note.value("category").toString() << "\n";
    s << "- Review date: " << note.value("reviewDate").toString() << "\n";
    s << "- Tickers: " << note.value("tickers").toString() << "\n";
    s << "- Tags: " << note.value("tags").toString() << "\n";
    s << "- Sentiment: " << note.value("sentiment").toString() << "\n";
    s << "- Direction: " << note.value("direction").toString() << "\n\n";
    s << note.value("content").toString().trimmed() << "\n";
    return out;
}

bool NotesController::exportNote(int id) {
    if (!ensureDatabase() || id <= 0)
        return false;
    QSqlQuery q(db_);
    q.prepare("SELECT * FROM notes WHERE id=?");
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        setStatus("找不到要导出的旧笔记");
        return false;
    }
    const QVariantMap note = rowToNote(q);
    const QString path = AppPaths::exports() + "/" + slug(note.value("title").toString()) + ".md";
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus("导出失败：" + path);
        return false;
    }
    file.write(noteMarkdown(note).toUtf8());
    const bool ok = file.commit();
    setStatus(ok ? "已导出 Markdown：" + path : "Markdown 导出失败");
    return ok;
}

QString NotesController::monthMarkdown(const QString& yearMonth) const {
    QString out;
    QTextStream s(&out);
    s << "# MyQuant 复盘数据库 " << yearMonth << "\n\n";
    auto section = [&](const QString& title, const QVariantList& rows, const QStringList& fields) {
        s << "## " << title << "\n\n";
        if (rows.isEmpty()) {
            s << "_暂无记录_\n\n";
            return;
        }
        for (const QVariant& value : rows) {
            const QVariantMap row = value.toMap();
            s << "- ";
            QStringList parts;
            for (const QString& field : fields) {
                const QString text = row.value(field).toString();
                if (!text.isEmpty())
                    parts << field + ": " + text;
            }
            s << parts.join("；") << "\n";
        }
        s << "\n";
    };
    section("每日市场", marketDays_, {"review_date", "market_status", "market_theme", "emotion", "market_view"});
    section("个股/组合观察", watchlistItems_,
            {"review_date", "symbol", "name", "industry_theme", "watch_reason", "trigger_condition", "invalidation_condition"});
    section("交易执行", tradeExecutions_,
            {"trade_date", "symbol", "name", "action", "price", "position_pct", "trade_reasons", "actual_return",
             "error_attribution", "next_improvement"});
    section("周/月总结", periodSummaries_,
            {"period_type", "period_key", "total_return", "benchmark_return", "excess_return", "common_mistake",
             "next_rule", "notes"});
    return out;
}

bool NotesController::exportMonth(const QString& yearMonth) {
    loadMonth(yearMonth);
    const QString path = AppPaths::exports() + "/" + slug("review-" + activeMonth_) + ".md";
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus("月度复盘导出失败：" + path);
        return false;
    }
    file.write(monthMarkdown(activeMonth_).toUtf8());
    const bool ok = file.commit();
    setStatus(ok ? "已导出月度复盘：" + path : "月度复盘导出失败");
    return ok;
}
