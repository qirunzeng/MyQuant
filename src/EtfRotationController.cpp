#include "EtfRotationController.h"

#include "AppPaths.h"

#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <QTemporaryFile>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>

namespace {
constexpr int kLookbackDays = 20;
constexpr int kRetShortDays = 20;
constexpr int kRetMidDays = 60;
constexpr int kRetLongDays = 120;
constexpr int kMaFilterDays = 120;
constexpr int kFactorWarmupDays = 260;
constexpr int kWarmupCalendarDays = 380;
constexpr int kHoldTime = 10;
constexpr double kCommissionRate = 0.0005;
constexpr double kCommissionMin = 0.01;
constexpr double kRiskStdThreshold = 0.03;
constexpr double kRiskCvThreshold = 0.5;

QString etfConfigDir() { return AppPaths::etf() + "/config"; }
QString etfDataDir() { return AppPaths::etf() + "/data"; }
QString universePath() { return etfConfigDir() + "/etf_universe.csv"; }
QString barsPath() { return etfDataDir() + "/etf_daily_bars.csv"; }
QString accountPath() { return etfConfigDir() + "/account.json"; }
QString realPositionPath() { return etfConfigDir() + "/real_position.csv"; }
QString fetchMetaPath() { return etfDataDir() + "/fetch_meta.json"; }
QString legacyBarsPath() { return QDir::homePath() + "/Documents/量化/ETF/data/market/etf_daily_bars.csv"; }
QString legacyRealPositionPath() { return QDir::homePath() + "/Documents/量化/ETF/config/real_position.csv"; }

QString compactDate(QString value) {
    value = value.trimmed();
    value.remove('-');
    return value.size() == 8 ? value : QString();
}

QString prettyDate(const QString& compact) {
    if (compact.size() == 8)
        return compact.left(4) + "-" + compact.mid(4, 2) + "-" + compact.mid(6, 2);
    return compact;
}

QString addDaysCompact(const QString& compact, int days) {
    QDate date = QDate::fromString(prettyDate(compact), "yyyy-MM-dd");
    if (!date.isValid())
        return compact;
    return date.addDays(days).toString("yyyyMMdd");
}

QString previousBusinessDate() {
    QDate date = QDate::currentDate().addDays(-1);
    while (date.dayOfWeek() > 5)
        date = date.addDays(-1);
    return date.toString("yyyyMMdd");
}

QStringList splitCsvLine(const QString& line) {
    QStringList fields;
    QString current;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == '"') {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == '"') {
                current += '"';
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (ch == ',' && !quoted) {
            fields.append(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    fields.append(current);
    return fields;
}

QString csvCell(QString value) {
    value.replace('"', "\"\"");
    return (value.contains(',') || value.contains('"') || value.contains('\n')) ? "\"" + value + "\"" : value;
}

double toDouble(const QString& value, double fallback = 0.0) {
    bool ok = false;
    const double out = value.trimmed().toDouble(&ok);
    return ok && std::isfinite(out) ? out : fallback;
}

bool truthy(QString value) {
    value = value.trimmed().toLower();
    return !(value.isEmpty() || value == "0" || value == "false" || value == "no" || value == "off");
}

double sampleStd(const QVector<double>& values) {
    QVector<double> valid;
    valid.reserve(values.size());
    for (double v : values) {
        if (std::isfinite(v))
            valid.append(v);
    }
    if (valid.size() < 2)
        return std::numeric_limits<double>::quiet_NaN();
    const double avg = std::accumulate(valid.begin(), valid.end(), 0.0) / valid.size();
    double ss = 0.0;
    for (double v : valid)
        ss += (v - avg) * (v - avg);
    return std::sqrt(ss / (valid.size() - 1));
}

double meanWindow(const QVector<double>& values, int begin, int end) {
    if (begin < 0 || end >= values.size() || begin > end)
        return std::numeric_limits<double>::quiet_NaN();
    double sum = 0.0;
    for (int i = begin; i <= end; ++i) {
        if (!std::isfinite(values.at(i)))
            return std::numeric_limits<double>::quiet_NaN();
        sum += values.at(i);
    }
    return sum / (end - begin + 1);
}

double commission(double notional) {
    return notional <= 0.0 ? 0.0 : std::max(notional * kCommissionRate, kCommissionMin);
}

double proceedsAfterSell(double shares, double price) {
    if (shares <= 0.0 || price <= 0.0 || !std::isfinite(price))
        return 0.0;
    const double notional = shares * price;
    return notional - commission(notional);
}

struct BuyFill {
    double shares = 0.0;
    double fee = 0.0;
    double cashLeft = 0.0;
};

BuyFill cashAfterBuy(double budget, double price) {
    if (budget <= 0.0 || price <= 0.0 || !std::isfinite(price))
        return {};
    auto totalCost = [&](double shares) {
        const double notional = shares * price;
        return notional + commission(notional);
    };
    constexpr double lot = 100.0;
    double shares = std::floor(budget / (price * lot)) * lot;
    while (shares > 0.0 && totalCost(shares) > budget)
        shares -= lot;
    if (shares <= 0.0)
        return {};
    const double notional = shares * price;
    const double fee = commission(notional);
    return {shares, fee, budget - notional - fee};
}

QVector<QVariantMap> defaultUniverseRows() {
    return {
        {{"enabled", true}, {"code", "159915"}, {"name", "创业板ETF"}, {"asset_class", "equity_cn_growth"}, {"note", "growth board momentum"}},
        {{"enabled", true}, {"code", "510300"}, {"name", "沪深300ETF"}, {"asset_class", "equity_cn_large"}, {"note", "broad A-share core"}},
        {{"enabled", true}, {"code", "512890"}, {"name", "红利低波ETF"}, {"asset_class", "equity_cn_style"}, {"note", "dividend low-vol defensive equity"}},
        {{"enabled", true}, {"code", "159920"}, {"name", "恒生ETF"}, {"asset_class", "equity_hk"}, {"note", "Hong Kong equity proxy"}},
        {{"enabled", true}, {"code", "513520"}, {"name", "日经ETF"}, {"asset_class", "equity_jp"}, {"note", "Japan equity proxy"}},
        {{"enabled", true}, {"code", "513310"}, {"name", "中韩半导体ETF"}, {"asset_class", "equity_kr"}, {"note", "Korea semiconductor proxy"}},
        {{"enabled", true}, {"code", "513300"}, {"name", "纳斯达克ETF"}, {"asset_class", "equity_us"}, {"note", "US growth exposure"}},
        {{"enabled", true}, {"code", "518880"}, {"name", "黄金ETF"}, {"asset_class", "commodity"}, {"note", "gold hedge"}},
        {{"enabled", true}, {"code", "511580"}, {"name", "国债政金债ETF招商"}, {"asset_class", "bond"}, {"note", "policy-bank bond defensive asset"}},
        {{"enabled", true}, {"code", "561360"}, {"name", "石油ETF国泰"}, {"asset_class", "commodity"}, {"note", "oil commodity trend"}},
        {{"enabled", true}, {"code", "159985"}, {"name", "豆粕ETF华夏"}, {"asset_class", "commodity"}, {"note", "agriculture commodity trend"}},
    };
}

QString actionLabel(const QString& action) {
    if (action == "rebalance_buy")
        return "动量入选";
    if (action == "rebalance_sell")
        return "调仓卖出";
    if (action == "risk_half")
        return "风险半仓";
    if (action == "stop_loss")
        return "固定止损";
    return action;
}
}

EtfRotationController::EtfRotationController(SettingsController* settings, QObject* parent)
    : QObject(parent), settings_(settings) {
    AppPaths::ensureAll();
    ensureSeed();
    refresh();
    refreshPortfolio();
}

void EtfRotationController::setRunning(bool value) {
    if (running_ == value)
        return;
    running_ = value;
    emit runningChanged();
}

void EtfRotationController::setStatus(const QString& message) {
    if (statusMessage_ == message)
        return;
    statusMessage_ = message;
    emit statusMessageChanged();
}

QString EtfRotationController::normalizeCode(QString code) {
    code = code.trimmed().toUpper();
    code.remove(QRegularExpression("^ETF\\."));
    if (code.isEmpty())
        return {};
    if (code.endsWith(".SH") || code.endsWith(".SZ"))
        return code;
    if (code.startsWith('5') || code.startsWith('6'))
        return code + ".SH";
    return code + ".SZ";
}

QString EtfRotationController::displayCode(QString code) {
    code = normalizeCode(code);
    return code.left(6);
}

bool EtfRotationController::ensureSeed() {
    QDir().mkpath(etfConfigDir());
    QDir().mkpath(etfDataDir());

    if (!QFileInfo::exists(universePath())) {
        const QString seed = AppPaths::resourceFile("seed/etf_universe.csv");
        if (QFileInfo::exists(seed)) {
            QFile::copy(seed, universePath());
        } else {
            QSaveFile f(universePath());
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
                return false;
            f.write("enabled,code,name,asset_class,note\n");
            for (const QVariantMap& row : defaultUniverseRows()) {
                const QStringList line = {
                    row.value("enabled").toBool() ? "1" : "0",
                    csvCell(row.value("code").toString()),
                    csvCell(row.value("name").toString()),
                    csvCell(row.value("asset_class").toString()),
                    csvCell(row.value("note").toString()),
                };
                f.write(line.join(',').toUtf8());
                f.write("\n");
            }
            f.commit();
        }
    }

    if (!QFileInfo::exists(realPositionPath()) && QFileInfo::exists(legacyRealPositionPath())) {
        QFile::copy(legacyRealPositionPath(), realPositionPath());
    }
    if (!QFileInfo::exists(realPositionPath())) {
        QSaveFile f(realPositionPath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write("enabled,code,name,shares,cost_price,note\n");
            for (const QVariantMap& row : defaultUniverseRows()) {
                const QStringList line = {"0", csvCell(row.value("code").toString()),
                                          csvCell(row.value("name").toString()), "0", "",
                                          csvCell(row.value("note").toString())};
                f.write(line.join(',').toUtf8());
                f.write("\n");
            }
            f.commit();
        }
    }

    if (!QFileInfo::exists(accountPath())) {
        QSaveFile f(accountPath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(QJsonDocument(QJsonObject{{"availableCash", 40000.0}}).toJson(QJsonDocument::Indented));
            f.commit();
        }
    }
    return true;
}

QVariantList EtfRotationController::loadUniverseRows() const {
    QVariantList rows;
    QFile file(universePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return rows;
    QStringList headers = splitCsvLine(QString::fromUtf8(file.readLine()).trimmed());
    if (!headers.isEmpty())
        headers[0].remove(QChar(0xFEFF));
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty())
            continue;
        const QStringList fields = splitCsvLine(line);
        QVariantMap row;
        for (int i = 0; i < headers.size(); ++i)
            row.insert(headers.at(i), i < fields.size() ? fields.at(i) : QString());
        row["enabled"] = truthy(row.value("enabled", "1").toString());
        row["code"] = displayCode(row.value("code").toString());
        rows.append(row);
    }
    return rows;
}

void EtfRotationController::refresh() {
    ensureSeed();
    universe_ = loadUniverseRows();
    setStatus(QString("ETF 池已载入：%1 条").arg(universe_.size()));
    emit universeChanged();
}

bool EtfRotationController::saveUniverse(const QVariantList& rows) {
    QSaveFile file(universePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus("保存 ETF 池失败");
        return false;
    }
    file.write("enabled,code,name,asset_class,note\n");
    int enabled = 0;
    for (const QVariant& value : rows) {
        const QVariantMap row = value.toMap();
        const bool isEnabled = row.value("enabled").toBool();
        if (isEnabled)
            ++enabled;
        const QStringList line = {
            isEnabled ? "1" : "0",
            csvCell(displayCode(row.value("code").toString())),
            csvCell(row.value("name").toString()),
            csvCell(row.value("asset_class").toString()),
            csvCell(row.value("note").toString()),
        };
        file.write(line.join(',').toUtf8());
        file.write("\n");
    }
    if (enabled == 0) {
        setStatus("ETF 池至少需要启用一只 ETF");
        return false;
    }
    const bool ok = file.commit();
    refresh();
    setStatus(ok ? "ETF 池已保存" : "ETF 池保存失败");
    return ok;
}

QVariantMap EtfRotationController::loadPortfolio() const {
    QVariantMap portfolio;
    double availableCash = 40000.0;
    QFile account(accountPath());
    if (account.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonObject obj = QJsonDocument::fromJson(account.readAll()).object();
        availableCash = obj.value("availableCash").toDouble(availableCash);
    }

    QHash<QString, QVariantMap> byCode;
    QFile positionsFile(realPositionPath());
    if (positionsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QStringList headers = splitCsvLine(QString::fromUtf8(positionsFile.readLine()).trimmed());
        if (!headers.isEmpty())
            headers[0].remove(QChar(0xFEFF));
        while (!positionsFile.atEnd()) {
            const QString line = QString::fromUtf8(positionsFile.readLine()).trimmed();
            if (line.isEmpty())
                continue;
            const QStringList fields = splitCsvLine(line);
            QVariantMap row;
            for (int i = 0; i < headers.size(); ++i)
                row.insert(headers.at(i), i < fields.size() ? fields.at(i) : QString());
            const QString code = displayCode(row.value("code", row.value("ts_code")).toString());
            if (code.isEmpty())
                continue;
            row["enabled"] = truthy(row.value("enabled", "0").toString()) || toDouble(row.value("shares").toString()) > 0.0;
            row["code"] = code;
            row["shares"] = toDouble(row.value("shares").toString());
            row["cost_price"] = toDouble(row.value("cost_price").toString(), 0.0);
            byCode.insert(code, row);
        }
    }

    QVariantList positions;
    for (const QVariant& value : universe_) {
        const QVariantMap u = value.toMap();
        const QString code = displayCode(u.value("code").toString());
        QVariantMap row = byCode.value(code);
        if (row.isEmpty()) {
            row = {{"enabled", u.value("enabled").toBool()}, {"code", code}, {"name", u.value("name").toString()},
                   {"shares", 0.0}, {"cost_price", 0.0}, {"note", ""}};
        } else {
            row["enabled"] = u.value("enabled").toBool();
            if (row.value("name").toString().isEmpty())
                row["name"] = u.value("name").toString();
        }
        row["asset_class"] = u.value("asset_class").toString();
        if (row.value("note").toString().isEmpty())
            row["note"] = u.value("note").toString();
        positions.append(row);
    }
    portfolio["availableCash"] = availableCash;
    portfolio["positions"] = positions;
    return portfolio;
}

void EtfRotationController::refreshPortfolio() {
    ensureSeed();
    portfolio_ = loadPortfolio();
    emit portfolioChanged();
}

bool EtfRotationController::savePortfolio(double availableCash, const QVariantList& positions) {
    QVariantList nextUniverse;
    int enabledUniverse = 0;
    for (const QVariant& value : positions) {
        QVariantMap row = value.toMap();
        const QString code = displayCode(row.value("code").toString());
        if (code.isEmpty())
            continue;
        const bool enabled = row.value("enabled").toBool();
        if (enabled)
            ++enabledUniverse;
        nextUniverse.append(QVariantMap{
            {"enabled", enabled},
            {"code", code},
            {"name", row.value("name").toString()},
            {"asset_class", row.value("asset_class", "unknown").toString()},
            {"note", row.value("note").toString()},
        });
    }
    if (enabledUniverse == 0) {
        setStatus("ETF 回测池至少需要启用一只标的");
        return false;
    }
    if (!saveUniverse(nextUniverse))
        return false;

    QSaveFile account(accountPath());
    if (!account.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus("保存账户现金失败");
        return false;
    }
    account.write(QJsonDocument(QJsonObject{{"availableCash", std::max(0.0, availableCash)}}).toJson(QJsonDocument::Indented));
    if (!account.commit()) {
        setStatus("保存账户现金失败");
        return false;
    }

    QSaveFile file(realPositionPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus("保存真实持仓失败");
        return false;
    }
    file.write("enabled,code,name,shares,cost_price,note\n");
    for (const QVariant& value : positions) {
        const QVariantMap row = value.toMap();
        const QString code = displayCode(row.value("code").toString());
        if (code.isEmpty())
            continue;
        const double shares = std::max(0.0, row.value("shares").toDouble());
        const bool hasRealPosition = shares > 0.0;
        const QStringList line = {
            hasRealPosition ? "1" : "0",
            csvCell(code),
            csvCell(row.value("name").toString()),
            QString::number(shares, 'f', 4),
            row.value("cost_price").toDouble() > 0 ? QString::number(row.value("cost_price").toDouble(), 'f', 4) : QString(),
            csvCell(row.value("note").toString()),
        };
        file.write(line.join(',').toUtf8());
        file.write("\n");
    }
    const bool ok = file.commit();
    refresh();
    refreshPortfolio();
    setStatus(ok ? "回测池、现金与真实持仓已保存" : "真实持仓保存失败");
    return ok;
}

QList<EtfRotationController::Bar> EtfRotationController::loadBarsFromPath(const QString& path) const {
    QList<Bar> out;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    QStringList headers = splitCsvLine(QString::fromUtf8(file.readLine()).trimmed());
    if (!headers.isEmpty())
        headers[0].remove(QChar(0xFEFF));
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty())
            continue;
        const QStringList fields = splitCsvLine(line);
        QHash<QString, QString> row;
        for (int i = 0; i < headers.size(); ++i)
            row.insert(headers.at(i), i < fields.size() ? fields.at(i) : QString());
        Bar bar;
        bar.code = normalizeCode(row.value("ts_code", row.value("code")));
        bar.date = compactDate(row.value("trade_date", row.value("date")));
        bar.open = toDouble(row.value("open"));
        bar.high = toDouble(row.value("high"));
        bar.low = toDouble(row.value("low"));
        bar.close = toDouble(row.value("close"));
        bar.vol = toDouble(row.value("vol", row.value("volume")));
        bar.amount = toDouble(row.value("amount"), bar.vol * bar.close);
        if (!bar.code.isEmpty() && bar.date.size() == 8 && bar.close > 0.0 && bar.open > 0.0 &&
            bar.high > 0.0 && bar.low > 0.0) {
            out.append(bar);
        }
    }
    return out;
}

QList<EtfRotationController::Bar> EtfRotationController::loadBars() const {
    return loadBarsFromPath(barsPath());
}

bool EtfRotationController::importLegacyBarsIfAvailable() {
    const QString source = legacyBarsPath();
    if (!QFileInfo::exists(source))
        return false;
    QFile::remove(barsPath());
    const bool ok = QFile::copy(source, barsPath());
    if (ok)
        setStatus("已导入原 ETF 项目的 qfq 日线数据");
    return ok;
}

QVariantList EtfRotationController::collectDataIssues(const QList<Bar>& bars) const {
    QVariantList issues;
    QHash<QString, QString> nameByCode;
    for (const QVariant& value : universe_) {
        const QVariantMap row = value.toMap();
        nameByCode.insert(normalizeCode(row.value("code").toString()), row.value("name").toString());
    }

    QHash<QString, QVector<Bar>> byCode;
    for (const Bar& bar : bars)
        byCode[bar.code].append(bar);

    for (auto it = byCode.begin(); it != byCode.end(); ++it) {
        QVector<Bar>& rows = it.value();
        std::sort(rows.begin(), rows.end(), [](const Bar& a, const Bar& b) { return a.date < b.date; });
        if (rows.size() < 2)
            continue;
        const double first = rows.first().close;
        const double last = rows.last().close;
        if (first > 0.0) {
            const double ratio = last / first;
            if (ratio > 20.0 || ratio < 0.03) {
                issues.append(QVariantMap{
                    {"type", "price_range"},
                    {"severity", "block"},
                    {"symbol", displayCode(it.key())},
                    {"name", nameByCode.value(it.key(), it.key())},
                    {"date", prettyDate(rows.last().date)},
                    {"return", ratio - 1.0},
                    {"message", "整段价格倍数异常，可能混入未复权或旧演示数据"},
                });
            }
        }
        for (int i = 1; i < rows.size(); ++i) {
            if (rows.at(i - 1).close <= 0.0)
                continue;
            const double ret = rows.at(i).close / rows.at(i - 1).close - 1.0;
            if (std::fabs(ret) > 0.20) {
                issues.append(QVariantMap{
                    {"type", "single_day_gap"},
                    {"severity", "block"},
                    {"symbol", displayCode(rows.at(i).code)},
                    {"name", nameByCode.value(rows.at(i).code, rows.at(i).code)},
                    {"date", prettyDate(rows.at(i).date)},
                    {"previousDate", prettyDate(rows.at(i - 1).date)},
                    {"previousClose", rows.at(i - 1).close},
                    {"close", rows.at(i).close},
                    {"return", ret},
                    {"message", "单日跳变超过 20%，可能是分红/除权/复权源混用造成的假回撤"},
                });
                if (issues.size() >= 24)
                    return issues;
            }
        }
    }
    return issues;
}

bool EtfRotationController::barsLookSuspicious(const QList<Bar>& bars) const {
    return !collectDataIssues(bars).isEmpty();
}

bool EtfRotationController::hasLocalDataCoverage(const QList<Bar>& bars, const QString& startDate, const QString& endDate) const {
    if (bars.isEmpty())
        return false;
    QSet<QString> enabledCodes;
    for (const QVariant& value : universe_) {
        const QVariantMap row = value.toMap();
        if (row.value("enabled").toBool())
            enabledCodes.insert(normalizeCode(row.value("code").toString()));
    }
    if (enabledCodes.isEmpty())
        return false;

    bool metaCoversRequest = false;
    QFile metaFile(fetchMetaPath());
    if (metaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonObject meta = QJsonDocument::fromJson(metaFile.readAll()).object();
        const QString metaStart = meta.value("lastFetchStart").toString(meta.value("start").toString());
        const QString metaEnd = meta.value("lastFetchEnd").toString(meta.value("end").toString());
        const QStringList metaCodeList = meta.value("codes").toString().split(',', Qt::SkipEmptyParts);
        const QSet<QString> metaCodes(metaCodeList.begin(), metaCodeList.end());
        bool codesCovered = true;
        for (const QString& code : enabledCodes) {
            if (!metaCodes.contains(displayCode(code))) {
                codesCovered = false;
                break;
            }
        }
        metaCoversRequest = !metaStart.isEmpty() && !metaEnd.isEmpty() && metaStart <= startDate &&
                            metaEnd >= endDate && codesCovered;
    }

    struct Range {
        QString first;
        QString last;
        int count = 0;
    };
    QHash<QString, Range> ranges;
    for (const Bar& bar : bars) {
        if (!enabledCodes.contains(bar.code))
            continue;
        Range& range = ranges[bar.code];
        if (range.first.isEmpty() || bar.date < range.first)
            range.first = bar.date;
        if (range.last.isEmpty() || bar.date > range.last)
            range.last = bar.date;
        ++range.count;
    }
    QStringList lateStartCodes;
    for (const QString& code : enabledCodes) {
        const Range range = ranges.value(code);
        if (range.count < 40 || range.last < endDate)
            return false;
        if (range.first > startDate)
            lateStartCodes.append(code);
    }
    return lateStartCodes.isEmpty() || metaCoversRequest;
}

bool EtfRotationController::fetchLiveData(const QString& startDate, const QString& endDate) {
    const QList<Bar> cachedBars = loadBars();
    if (hasLocalDataCoverage(cachedBars, startDate, endDate)) {
        setStatus("本地 qfq 日线已覆盖当前区间，跳过 AkShare 下载");
        return true;
    }

    const QString script = AppPaths::resourceFile("scripts/akshare_fetch.py");
    if (!QFileInfo::exists(script)) {
        setStatus("找不到 AkShare helper");
        return false;
    }
    const QString python = settings_ ? settings_->pythonPath() : "python3";
    const QString sources = settings_ ? settings_->dataSources() : "em,tx,sina";
    QStringList enabledCodes;
    for (const QVariant& value : universe_) {
        const QVariantMap row = value.toMap();
        if (row.value("enabled").toBool())
            enabledCodes.append(displayCode(row.value("code").toString()));
    }
    std::sort(enabledCodes.begin(), enabledCodes.end());
    const QString requestKey = startDate + "|" + endDate + "|" + sources + "|" + enabledCodes.join(',');

    QTemporaryFile temp(QDir::tempPath() + "/myquant-akshare-XXXXXX.csv");
    if (!temp.open()) {
        setStatus("创建 AkShare 临时缓存失败");
        return false;
    }
    const QString tempPath = temp.fileName();
    temp.close();

    QProcess process;
    const QStringList args = {
        script,
        "--universe", universePath(),
        "--output", tempPath,
        "--start", startDate,
        "--end", endDate,
        "--adjust", "qfq",
        "--sources", sources,
    };
    process.start(python, args);
    if (!process.waitForFinished(240000)) {
        process.kill();
        setStatus("AkShare 更新超时");
        return false;
    }
    if (process.exitCode() != 0) {
        const QString error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        setStatus("AkShare 更新失败：" + error.left(220));
        return false;
    }

    const QList<Bar> freshBars = loadBarsFromPath(tempPath);
    if (freshBars.isEmpty()) {
        setStatus("AkShare 更新未返回可用 qfq 日线");
        return false;
    }
    QHash<QString, Bar> mergedByKey;
    auto keyOf = [](const Bar& bar) { return bar.code + "|" + bar.date; };
    for (const Bar& bar : cachedBars)
        mergedByKey.insert(keyOf(bar), bar);
    for (const Bar& bar : freshBars)
        mergedByKey.insert(keyOf(bar), bar);
    QList<Bar> mergedBars = mergedByKey.values();
    std::sort(mergedBars.begin(), mergedBars.end(), [](const Bar& a, const Bar& b) {
        if (a.date != b.date)
            return a.date < b.date;
        return a.code < b.code;
    });
    if (!writeBars(mergedBars)) {
        setStatus("写入本地 qfq 日线缓存失败");
        return false;
    }

    QSaveFile metaOut(fetchMetaPath());
    if (metaOut.open(QIODevice::WriteOnly | QIODevice::Text)) {
        metaOut.write(QJsonDocument(QJsonObject{{"requestKey", requestKey},
                                                {"lastFetchStart", startDate},
                                                {"lastFetchEnd", endDate},
                                                {"sources", sources},
                                                {"codes", enabledCodes.join(',')},
                                                {"cachedRows", mergedBars.size()},
                                                {"updatedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}})
                          .toJson(QJsonDocument::Indented));
        metaOut.commit();
    }
    setStatus(QString("AkShare qfq 日线已合并进本地缓存：新增/更新 %1 行，缓存共 %2 行")
                  .arg(freshBars.size())
                  .arg(mergedBars.size()));
    return true;
}

bool EtfRotationController::writeBars(const QList<Bar>& bars) const {
    QSaveFile file(barsPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write("ts_code,trade_date,open,high,low,close,vol,amount,adj_factor\n");
    for (const Bar& b : bars) {
        const QStringList line = {
            b.code,
            b.date,
            QString::number(b.open, 'f', 4),
            QString::number(b.high, 'f', 4),
            QString::number(b.low, 'f', 4),
            QString::number(b.close, 'f', 4),
            QString::number(b.vol, 'f', 2),
            QString::number(b.amount, 'f', 2),
            "1",
        };
        file.write(line.join(',').toUtf8());
        file.write("\n");
    }
    return file.commit();
}

bool EtfRotationController::generateDemoData(const QString& startDate, const QString& endDate) {
    QVariantList rows = loadUniverseRows();
    QList<Bar> bars;
    QDate start = QDate::fromString(prettyDate(startDate), "yyyy-MM-dd");
    QDate end = QDate::fromString(prettyDate(endDate), "yyyy-MM-dd");
    if (!start.isValid())
        start = QDate::currentDate().addYears(-3);
    if (!end.isValid() || end <= start)
        end = QDate::currentDate().addDays(-1);

    int assetIndex = 0;
    for (const QVariant& value : rows) {
        const QVariantMap row = value.toMap();
        if (!row.value("enabled").toBool())
            continue;
        const QString code = normalizeCode(row.value("code").toString());
        double price = 1.0 + assetIndex * 0.18;
        int day = 0;
        for (QDate d = start; d <= end; d = d.addDays(1)) {
            if (d.dayOfWeek() > 5)
                continue;
            const double drift = 0.00002 * ((assetIndex % 5) - 2);
            const double cycle = 0.025 * (std::sin((day + assetIndex * 9) / 32.0) -
                                          std::sin((day - 1 + assetIndex * 9) / 32.0));
            const double noise = 0.0025 * std::sin(day * 1.7 + assetIndex * 0.9);
            const double ret = std::clamp(drift + cycle + noise, -0.025, 0.025);
            price = std::max(0.25, price * (1.0 + ret));
            Bar b;
            b.code = code;
            b.date = d.toString("yyyyMMdd");
            b.close = price;
            b.open = price * (1.0 - 0.0015);
            b.high = price * (1.0 + 0.005);
            b.low = price * (1.0 - 0.005);
            b.vol = 1000000.0 + assetIndex * 100000.0;
            b.amount = 100000000.0 + assetIndex * 8000000.0 + day * 30000.0;
            bars.append(b);
            ++day;
        }
        ++assetIndex;
    }
    const bool ok = writeBars(bars);
    setStatus(ok ? "已生成测试用演示数据；实盘复盘会自动判断是否补齐 AkShare 数据" : "演示 ETF 数据生成失败");
    return ok;
}

double EtfRotationController::weightedMomentum(const QVector<double>& closes, int index) {
    const int start = index - kLookbackDays;
    if (start < 0)
        return std::numeric_limits<double>::quiet_NaN();
    const int n = kLookbackDays + 1;
    double sw = 0.0;
    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double sxy = 0.0;
    QVector<double> y;
    QVector<double> weights;
    y.reserve(n);
    weights.reserve(n);
    for (int j = 0; j < n; ++j) {
        const double close = closes.at(start + j);
        if (!std::isfinite(close) || close <= 0.0)
            return std::numeric_limits<double>::quiet_NaN();
        const double x = j;
        const double yy = std::log(close);
        const double w = 1.0 + double(j) / double(n - 1);
        y.append(yy);
        weights.append(w);
        sw += w;
        sx += w * x;
        sy += w * yy;
        sxx += w * x * x;
        sxy += w * x * yy;
    }
    const double denom = sw * sxx - sx * sx;
    if (std::fabs(denom) < 1e-15)
        return std::numeric_limits<double>::quiet_NaN();
    const double slope = (sw * sxy - sx * sy) / denom;
    const double intercept = (sy - slope * sx) / sw;
    const double yMean = std::accumulate(y.begin(), y.end(), 0.0) / y.size();
    double ssRes = 0.0;
    double ssTot = 0.0;
    for (int j = 0; j < n; ++j) {
        const double fitted = slope * j + intercept;
        ssRes += weights.at(j) * (y.at(j) - fitted) * (y.at(j) - fitted);
        ssTot += weights.at(j) * (y.at(j) - yMean) * (y.at(j) - yMean);
    }
    const double r2 = ssTot != 0.0 ? std::max(0.0, 1.0 - ssRes / ssTot) : 0.0;
    return (std::exp(slope * 250.0) - 1.0) * r2;
}

QVariantMap EtfRotationController::runBacktest(QList<Bar> bars, const QString& startDate, const QString& endDate,
                                               int holdNum, double targetCashRatio,
                                               double stopLossRate, double initialCapital) {
    QHash<QString, QString> nameByCode;
    QHash<QString, QString> assetByCode;
    QSet<QString> enabledCodes;
    for (const QVariant& value : universe_) {
        const QVariantMap row = value.toMap();
        if (!row.value("enabled").toBool())
            continue;
        const QString code = normalizeCode(row.value("code").toString());
        enabledCodes.insert(code);
        nameByCode.insert(code, row.value("name").toString());
        assetByCode.insert(code, row.value("asset_class").toString());
    }
    bars.erase(std::remove_if(bars.begin(), bars.end(), [&](const Bar& bar) {
                   return !enabledCodes.contains(bar.code);
               }),
               bars.end());
    if (bars.isEmpty())
        return {{"error", "启用 ETF 没有可用日线数据，请先更新 AkShare 数据"}};

    QHash<QString, QVector<Bar>> byCode;
    for (const Bar& bar : bars)
        byCode[bar.code].append(bar);
    for (auto it = byCode.begin(); it != byCode.end(); ++it) {
        QVector<Bar>& series = it.value();
        std::sort(series.begin(), series.end(), [](const Bar& a, const Bar& b) { return a.date < b.date; });
        QVector<double> closes;
        QVector<double> amounts;
        QVector<double> dailyReturns;
        closes.reserve(series.size());
        amounts.reserve(series.size());
        dailyReturns.resize(series.size());
        for (int i = 0; i < series.size(); ++i) {
            closes.append(series.at(i).close);
            amounts.append(series.at(i).amount);
            dailyReturns[i] = std::numeric_limits<double>::quiet_NaN();
            if (i > 0 && closes.at(i - 1) > 0.0)
                dailyReturns[i] = closes.at(i) / closes.at(i - 1) - 1.0;
        }
        for (int i = 0; i < series.size(); ++i) {
            Bar& b = series[i];
            b.momentum = weightedMomentum(closes, i);
            b.stdScore = std::numeric_limits<double>::quiet_NaN();
            b.cvScore = std::numeric_limits<double>::quiet_NaN();
            b.ret20 = std::numeric_limits<double>::quiet_NaN();
            b.ret60 = std::numeric_limits<double>::quiet_NaN();
            b.ret120 = std::numeric_limits<double>::quiet_NaN();
            b.ma120 = std::numeric_limits<double>::quiet_NaN();
            b.dualMomentum = std::numeric_limits<double>::quiet_NaN();
            b.combinedMomentum = std::numeric_limits<double>::quiet_NaN();
            if (i >= 20) {
                QVector<double> r20;
                QVector<double> a20;
                r20.reserve(20);
                a20.reserve(20);
                for (int j = i - 19; j <= i; ++j) {
                    r20.append(dailyReturns.at(j));
                    a20.append(amounts.at(j));
                }
                const double avgAmount = std::accumulate(a20.begin(), a20.end(), 0.0) / a20.size();
                b.cvScore = avgAmount != 0.0 ? sampleStd(a20) / avgAmount : std::numeric_limits<double>::quiet_NaN();
                const double s20 = sampleStd(r20);
                QVector<double> r5;
                for (int j = i - 4; j <= i; ++j)
                    r5.append(dailyReturns.at(j));
                const double s5 = sampleStd(r5);
                if (std::isfinite(s20) && std::isfinite(s5))
                    b.stdScore = (s20 + s5) / 2.0;
            }
            auto pct = [&](int days) {
                if (i < days || closes.at(i - days) <= 0.0)
                    return std::numeric_limits<double>::quiet_NaN();
                return closes.at(i) / closes.at(i - days) - 1.0;
            };
            b.ret20 = pct(kRetShortDays);
            b.ret60 = pct(kRetMidDays);
            b.ret120 = pct(kRetLongDays);
            if (i >= kMaFilterDays - 1)
                b.ma120 = meanWindow(closes, i - kMaFilterDays + 1, i);
            b.trendPass = std::isfinite(b.ma120) && b.close > b.ma120 && b.ret60 > 0.0 && b.ret120 > 0.0;
            if (std::isfinite(b.ret20) && std::isfinite(b.ret60) && std::isfinite(b.ret120))
                b.dualMomentum = 0.2 * b.ret20 + 0.3 * b.ret60 + 0.5 * b.ret120;
            if (std::isfinite(b.momentum) && std::isfinite(b.dualMomentum))
                b.combinedMomentum = 0.5 * b.momentum + 0.5 * b.dualMomentum;
        }
    }

    auto hasFactors = [](const Bar& bar) {
        return std::isfinite(bar.momentum) && std::isfinite(bar.stdScore) && std::isfinite(bar.cvScore) &&
               std::isfinite(bar.ret20) && std::isfinite(bar.ret60) && std::isfinite(bar.ret120) &&
               std::isfinite(bar.ma120) && std::isfinite(bar.dualMomentum) && std::isfinite(bar.combinedMomentum);
    };

    QMap<QString, QHash<QString, Bar>> priceByDateCode;
    QMap<QString, QHash<QString, Bar>> factorByDateCode;
    for (auto it = byCode.constBegin(); it != byCode.constEnd(); ++it) {
        for (const Bar& bar : it.value()) {
            if (bar.date < startDate || bar.date > endDate)
                continue;
            priceByDateCode[bar.date].insert(bar.code, bar);
            if (hasFactors(bar))
                factorByDateCode[bar.date].insert(bar.code, bar);
        }
    }
    if (priceByDateCode.size() < 2)
        return {{"error", "回测区间没有足够价格数据；请先补齐 AkShare 数据"}};
    const QDate requestedStartDate = QDate::fromString(prettyDate(startDate), "yyyy-MM-dd");
    const QDate firstPriceDate = QDate::fromString(prettyDate(priceByDateCode.firstKey()), "yyyy-MM-dd");
    if (requestedStartDate.isValid() && firstPriceDate.isValid() && firstPriceDate > requestedStartDate.addDays(10)) {
        return {{"error", QString("本地缓存最早可用交易日为 %1，无法从 %2 开始；请检查 AkShare/Python 设置后重新运行")
                              .arg(prettyDate(priceByDateCode.firstKey()), prettyDate(startDate))}};
    }
    if (factorByDateCode.isEmpty())
        return {{"error", "ETF 因子数据不足；请补齐开始日期之前的预热数据"}};

    auto carryForwardRows = [&](QMap<QString, QHash<QString, Bar>>& table) {
        QHash<QString, Bar> lastSeen;
        for (auto it = table.begin(); it != table.end(); ++it) {
            for (auto bit = it.value().constBegin(); bit != it.value().constEnd(); ++bit)
                lastSeen.insert(bit.key(), bit.value());
            for (const QString& code : enabledCodes) {
                if (!it.value().contains(code) && lastSeen.contains(code)) {
                    Bar carried = lastSeen.value(code);
                    carried.date = it.key();
                    it.value().insert(code, carried);
                }
            }
        }
    };
    carryForwardRows(priceByDateCode);
    carryForwardRows(factorByDateCode);

    QStringList dates;
    for (auto it = priceByDateCode.constBegin(); it != priceByDateCode.constEnd(); ++it)
        dates.append(it.key());

    auto scoreOf = [](const Bar& bar) { return bar.momentum; };
    auto rankedBars = [&](const QHash<QString, Bar>& rows) {
        QVector<Bar> ranked;
        for (const Bar& bar : rows) {
            const double score = scoreOf(bar);
            if (std::isfinite(score))
                ranked.append(bar);
        }
        std::sort(ranked.begin(), ranked.end(), [&](const Bar& a, const Bar& b) {
            const double sa = scoreOf(a);
            const double sb = scoreOf(b);
            if (sa != sb)
                return sa > sb;
            return a.code < b.code;
        });
        return ranked;
    };
    auto selectWinners = [&](const QHash<QString, Bar>& rows) {
        QStringList winners;
        for (const Bar& bar : rankedBars(rows)) {
            if (winners.size() >= holdNum)
                break;
            if (scoreOf(bar) > 0.0)
                winners.append(bar.code);
        }
        return winners;
    };

    struct Event {
        QString signalDate;
        QStringList winners;
    };
    QHash<QString, Event> events;
    for (int i = 0; i + 1 < dates.size(); ++i) {
        if (i % kHoldTime != 0)
            continue;
        const QString signalDate = dates.at(i);
        if (!factorByDateCode.contains(signalDate))
            continue;
        events.insert(dates.at(i + 1), Event{signalDate, selectWinners(factorByDateCode.value(signalDate))});
    }

    struct SimPosition {
        double shares = 0.0;
        double buyPrice = std::numeric_limits<double>::quiet_NaN();
        double peakPrice = std::numeric_limits<double>::quiet_NaN();
        QString buyDate;
    };
    QHash<QString, SimPosition> positions;
    QSet<QString> riskHalfTriggered;
    double cash = initialCapital;
    double peak = initialCapital;
    QStringList pendingRiskHalfCodes;
    QStringList pendingStopCodes;
    QString pendingRiskSignalDate;
    QString pendingStopSignalDate;
    QVariantList outEquity;
    QVariantList outTrades;
    int rebalanceCount = 0;
    int riskHalfCount = 0;
    int stopCount = 0;

    auto execPrice = [&](const QString& code, const QString& date) {
        const QHash<QString, Bar> rows = priceByDateCode.value(date);
        if (!rows.contains(code))
            return 0.0;
        const Bar b = rows.value(code);
        return (b.open + b.high + b.low + b.close) / 4.0;
    };
    auto closeAt = [&](const QString& code, const QString& date) {
        const QHash<QString, Bar> rows = priceByDateCode.value(date);
        return rows.contains(code) ? rows.value(code).close : 0.0;
    };
    auto navValue = [&](const QString& date) {
        double value = cash;
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
            value += it.value().shares * closeAt(it.key(), date);
        return value;
    };
    auto holdingList = [&]() {
        QStringList codes = positions.keys();
        std::sort(codes.begin(), codes.end());
        return codes.join(',');
    };
    auto addTrade = [&](const QString& execDate, const QString& signalDate, const QString& code,
                        const QString& side, double price, double shares, const QString& action,
                        double cashBefore, double navAfter) {
        outTrades.append(QVariantMap{
            {"date", prettyDate(execDate)},
            {"signalDate", prettyDate(signalDate)},
            {"symbol", displayCode(code)},
            {"name", nameByCode.value(code, code)},
            {"side", side},
            {"price", price},
            {"shares", shares},
            {"reason", actionLabel(action)},
            {"cashBefore", cashBefore},
            {"navAfter", navAfter},
            {"holdings", holdingList()},
        });
    };

    for (int di = 0; di < dates.size(); ++di) {
        const QString date = dates.at(di);
        const QHash<QString, Bar> today = priceByDateCode.value(date);

        if (!pendingRiskHalfCodes.isEmpty()) {
            for (const QString& code : std::as_const(pendingRiskHalfCodes)) {
                if (!positions.contains(code))
                    continue;
                const double px = execPrice(code, date);
                if (px <= 0.0)
                    continue;
                SimPosition pos = positions.value(code);
                const double sellShares = pos.shares / 2.0;
                if (sellShares <= 0.0)
                    continue;
                const double before = cash;
                cash += proceedsAfterSell(sellShares, px);
                pos.shares -= sellShares;
                positions.insert(code, pos);
                riskHalfTriggered.insert(code);
                ++riskHalfCount;
                addTrade(date, pendingRiskSignalDate, code, "SELL", px, sellShares, "risk_half", before, navValue(date));
            }
            pendingRiskHalfCodes.clear();
        }

        if (!pendingStopCodes.isEmpty()) {
            for (const QString& code : std::as_const(pendingStopCodes)) {
                if (!positions.contains(code))
                    continue;
                const double px = execPrice(code, date);
                if (px <= 0.0)
                    continue;
                const SimPosition pos = positions.take(code);
                const double before = cash;
                cash += proceedsAfterSell(pos.shares, px);
                riskHalfTriggered.remove(code);
                ++stopCount;
                addTrade(date, pendingStopSignalDate, code, "SELL", px, pos.shares, "stop_loss", before, navValue(date));
            }
            pendingStopCodes.clear();
        }

        const auto ev = events.constFind(date);
        if (ev != events.constEnd()) {
            ++rebalanceCount;
            const QStringList winners = ev->winners;
            const QSet<QString> targetSet(winners.begin(), winners.end());
            QStringList toSell;
            for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
                if (!targetSet.contains(it.key()))
                    toSell.append(it.key());
            }
            for (const QString& code : toSell) {
                const double px = execPrice(code, date);
                if (px <= 0.0)
                    continue;
                const SimPosition pos = positions.take(code);
                const double before = cash;
                cash += proceedsAfterSell(pos.shares, px);
                riskHalfTriggered.remove(code);
                addTrade(date, ev->signalDate, code, "SELL", px, pos.shares, "rebalance_sell", before, navValue(date));
            }

            QStringList toBuy;
            for (const QString& code : winners) {
                if (!positions.contains(code))
                    toBuy.append(code);
            }
            const double navBeforeBuy = navValue(date);
            const double currentEquity = std::max(0.0, navBeforeBuy - cash);
            const double targetExposure = std::clamp(1.0 - targetCashRatio, 0.0, 1.0);
            const double targetEquity = navBeforeBuy * targetExposure;
            const double buyBudget = std::min(cash, std::max(0.0, targetEquity - currentEquity));
            const double perCash = toBuy.isEmpty() ? 0.0 : buyBudget / toBuy.size();
            const QHash<QString, Bar> signalBars = factorByDateCode.value(ev->signalDate);
            for (const QString& code : toBuy) {
                const double px = execPrice(code, date);
                if (px <= 0.0)
                    continue;
                const Bar signalBar = signalBars.value(code);
                const bool highRisk = std::isfinite(signalBar.stdScore) && std::isfinite(signalBar.cvScore) &&
                                      (signalBar.stdScore > kRiskStdThreshold || signalBar.cvScore > kRiskCvThreshold);
                const double budget = perCash * (highRisk ? 0.5 : 1.0);
                const double before = cash;
                const BuyFill fill = cashAfterBuy(budget, px);
                if (fill.shares <= 0.0)
                    continue;
                cash -= (budget - fill.cashLeft);
                positions.insert(code, SimPosition{fill.shares, px, px, date});
                addTrade(date, ev->signalDate, code, "BUY", px, fill.shares, "rebalance_buy", before, navValue(date));
            }
        }

        if (di + 1 < dates.size()) {
            pendingRiskHalfCodes.clear();
            pendingStopCodes.clear();
            for (auto it = positions.begin(); it != positions.end(); ++it) {
                const QString code = it.key();
                if (today.contains(code) && std::isfinite(today.value(code).close)) {
                    if (!std::isfinite(it.value().peakPrice) || today.value(code).close > it.value().peakPrice)
                        it.value().peakPrice = today.value(code).close;
                }
                if (today.contains(code) && !riskHalfTriggered.contains(code)) {
                    const Bar b = today.value(code);
                    if (std::isfinite(b.stdScore) && std::isfinite(b.cvScore) &&
                        (b.stdScore > kRiskStdThreshold || b.cvScore > kRiskCvThreshold)) {
                        pendingRiskHalfCodes.append(code);
                        pendingRiskSignalDate = date;
                    }
                }
                if (today.contains(code) && std::isfinite(it.value().buyPrice) && it.value().buyPrice > 0.0) {
                    const double ret = today.value(code).close / it.value().buyPrice - 1.0;
                    if (ret <= -stopLossRate) {
                        pendingStopCodes.append(code);
                        pendingStopSignalDate = date;
                    }
                }
            }
        }

        cash += cash * (0.01 / 365.0);
        const double nav = navValue(date);
        peak = std::max(peak, nav);
        const double drawdown = peak > 0.0 ? nav / peak - 1.0 : 0.0;
        outEquity.append(QVariantMap{{"date", prettyDate(date)}, {"nav", nav / initialCapital},
                                     {"equity", nav}, {"drawdown", drawdown},
                                     {"cashRatio", nav > 0.0 ? cash / nav : 0.0}});
    }

    if (outEquity.size() < 2)
        return {{"error", "回测交易日不足"}};

    const double startNav = outEquity.first().toMap().value("equity").toDouble();
    const double endNav = outEquity.last().toMap().value("equity").toDouble();
    const double totalReturn = startNav > 0.0 ? endNav / startNav - 1.0 : 0.0;
    const double years = std::max(1.0 / 252.0, double(outEquity.size() - 1) / 252.0);
    const double annReturn = std::pow(std::max(0.000001, 1.0 + totalReturn), 1.0 / years) - 1.0;
    QVector<double> rets;
    for (int i = 1; i < outEquity.size(); ++i) {
        const double prev = outEquity.at(i - 1).toMap().value("equity").toDouble();
        const double cur = outEquity.at(i).toMap().value("equity").toDouble();
        if (prev > 0.0)
            rets.append(cur / prev - 1.0);
    }
    const double avgRet = rets.isEmpty() ? 0.0 : std::accumulate(rets.begin(), rets.end(), 0.0) / rets.size();
    double var = 0.0;
    for (double r : rets)
        var += (r - avgRet) * (r - avgRet);
    if (rets.size() > 1)
        var /= rets.size() - 1;
    const double vol = std::sqrt(var) * std::sqrt(252.0);
    double maxDd = 0.0;
    double avgCash = 0.0;
    double minCash = 1.0;
    double peakEquity = outEquity.first().toMap().value("equity").toDouble();
    int peakIndex = 0;
    int ddStartIndex = 0;
    int ddEndIndex = 0;
    QMap<QString, QPair<double, double>> monthlyEquity;
    for (int i = 0; i < outEquity.size(); ++i) {
        const QVariant& value = outEquity.at(i);
        const QVariantMap row = value.toMap();
        const double eq = row.value("equity").toDouble();
        if (eq > peakEquity) {
            peakEquity = eq;
            peakIndex = i;
        }
        const double dd = row.value("drawdown").toDouble();
        if (dd < maxDd) {
            maxDd = dd;
            ddStartIndex = peakIndex;
            ddEndIndex = i;
        }
        const double cashRatio = row.value("cashRatio").toDouble();
        avgCash += cashRatio;
        minCash = std::min(minCash, cashRatio);
        const QString month = row.value("date").toString().left(7);
        if (!month.isEmpty()) {
            if (!monthlyEquity.contains(month))
                monthlyEquity.insert(month, {eq, eq});
            else
                monthlyEquity[month].second = eq;
        }
    }
    avgCash /= outEquity.size();
    int recoveryDays = 0;
    if (ddEndIndex > ddStartIndex) {
        const double recoveryTarget = outEquity.at(ddStartIndex).toMap().value("equity").toDouble();
        const QDate bottomDate = QDate::fromString(outEquity.at(ddEndIndex).toMap().value("date").toString(), "yyyy-MM-dd");
        for (int i = ddEndIndex + 1; i < outEquity.size(); ++i) {
            if (outEquity.at(i).toMap().value("equity").toDouble() >= recoveryTarget) {
                const QDate recoveryDate = QDate::fromString(outEquity.at(i).toMap().value("date").toString(), "yyyy-MM-dd");
                if (bottomDate.isValid() && recoveryDate.isValid())
                    recoveryDays = bottomDate.daysTo(recoveryDate);
                break;
            }
        }
    }
    double worstMonth = 0.0;
    double bestMonth = 0.0;
    bool sawMonth = false;
    for (auto it = monthlyEquity.constBegin(); it != monthlyEquity.constEnd(); ++it) {
        if (it.value().first <= 0.0)
            continue;
        const double ret = it.value().second / it.value().first - 1.0;
        if (!sawMonth) {
            worstMonth = ret;
            bestMonth = ret;
            sawMonth = true;
        } else {
            worstMonth = std::min(worstMonth, ret);
            bestMonth = std::max(bestMonth, ret);
        }
    }

    const QString endPriceDate = dates.last();
    const double endCash = cash;
    const double endEquity = endNav;
    double backtestHoldingValue = 0.0;
    double backtestPnl = 0.0;
    QVariantList backtestHoldings;
    QStringList endCodes = positions.keys();
    std::sort(endCodes.begin(), endCodes.end());
    for (const QString& code : endCodes) {
        const SimPosition pos = positions.value(code);
        const double lastClose = closeAt(code, endPriceDate);
        if (pos.shares <= 0.0 || lastClose <= 0.0)
            continue;
        const double marketValue = pos.shares * lastClose;
        const double costValue = std::isfinite(pos.buyPrice) ? pos.shares * pos.buyPrice : 0.0;
        const double pnl = costValue > 0.0 ? marketValue - costValue : 0.0;
        backtestHoldingValue += marketValue;
        backtestPnl += pnl;
        backtestHoldings.append(QVariantMap{
            {"symbol", displayCode(code)},
            {"name", nameByCode.value(code, code)},
            {"shares", pos.shares},
            {"costPrice", std::isfinite(pos.buyPrice) ? pos.buyPrice : 0.0},
            {"lastPrice", lastClose},
            {"marketValue", marketValue},
            {"pnl", pnl},
            {"pnlRate", costValue > 0.0 ? pnl / costValue : 0.0},
            {"weight", endEquity > 0.0 ? marketValue / endEquity : 0.0},
            {"buyDate", prettyDate(pos.buyDate)},
            {"date", prettyDate(endPriceDate)},
        });
    }

    const QString latestDate = factorByDateCode.lastKey();
    const QHash<QString, Bar> latestBars = factorByDateCode.value(latestDate);
    const QStringList latestWinners = selectWinners(latestBars);
    const QSet<QString> latestWinnerSet(latestWinners.begin(), latestWinners.end());
    QVariantList outSignals;
    int rank = 1;
    for (const Bar& bar : rankedBars(latestBars)) {
        outSignals.append(QVariantMap{
            {"rank", rank++},
            {"symbol", displayCode(bar.code)},
            {"name", nameByCode.value(bar.code, bar.code)},
            {"assetClass", assetByCode.value(bar.code)},
            {"score", scoreOf(bar)},
            {"momentum", bar.momentum},
            {"stdScore", bar.stdScore},
            {"cvScore", bar.cvScore},
            {"ret20", bar.ret20},
            {"ret60", bar.ret60},
            {"ret120", bar.ret120},
            {"trendPass", bar.trendPass},
            {"close", bar.close},
            {"selected", latestWinnerSet.contains(bar.code)},
            {"date", prettyDate(latestDate)},
        });
    }

    QVariantMap outMetrics{
        {"startDate", outEquity.first().toMap().value("date")},
        {"endDate", outEquity.last().toMap().value("date")},
        {"cumulativeReturn", totalReturn},
        {"annualizedReturn", annReturn},
        {"annualizedVolatility", vol},
        {"sharpeRatio", vol > 1e-9 ? annReturn / vol : 0.0},
        {"maxDrawdown", maxDd},
        {"calmarRatio", maxDd < 0.0 ? annReturn / std::abs(maxDd) : 0.0},
        {"averageCashRatio", avgCash},
        {"minCashRatioObserved", minCash},
        {"worstMonthlyReturn", worstMonth},
        {"bestMonthlyReturn", bestMonth},
        {"recoveryDaysAfterMaxDrawdown", recoveryDays},
        {"turnover", years > 0.0 ? outTrades.size() / years : 0.0},
        {"tradeCount", outTrades.size()},
        {"rebalanceCount", rebalanceCount},
        {"riskHalfCount", riskHalfCount},
        {"stopCount", stopCount},
        {"backtestCash", endCash},
        {"backtestHoldingValue", backtestHoldingValue},
        {"backtestEquity", endEquity},
        {"backtestPnl", backtestPnl},
    };
    double realHoldingValue = 0.0;
    double realPnl = 0.0;
    const QVariantList realHoldings = buildRealHoldings(latestBars, &realHoldingValue, &realPnl);
    outMetrics.insert("realCash", std::max(0.0, portfolio_.value("availableCash").toDouble()));
    outMetrics.insert("realHoldingValue", realHoldingValue);
    outMetrics.insert("realPnl", realPnl);
    outMetrics.insert("realTotalAssets", outMetrics.value("realCash").toDouble() + realHoldingValue);
    return {{"metrics", outMetrics},
            {"equity", outEquity},
            {"trades", outTrades},
            {"rankings", outSignals},
            {"pricePoints", buildPricePoints(outTrades)},
            {"tradeCharts", buildTradeCharts(byCode, outTrades, startDate, endDate)},
            {"backtestHoldings", backtestHoldings},
            {"realHoldings", realHoldings},
            {"advice", buildAdvice(outSignals, latestBars, targetCashRatio, holdNum)}};
}

QVariantList EtfRotationController::buildRealHoldings(const QHash<QString, Bar>& latestBars, double* totalValue,
                                                      double* totalPnl) const {
    QVariantList out;
    if (totalValue)
        *totalValue = 0.0;
    if (totalPnl)
        *totalPnl = 0.0;

    const QVariantList positions = portfolio_.value("positions").toList();
    for (const QVariant& value : positions) {
        const QVariantMap row = value.toMap();
        if (!row.value("enabled", true).toBool())
            continue;
        const QString code = normalizeCode(row.value("code").toString());
        const double shares = row.value("shares").toDouble();
        if (code.isEmpty() || shares <= 0.0)
            continue;

        const Bar bar = latestBars.value(code);
        const double lastPrice = bar.close;
        const double marketValue = lastPrice > 0.0 ? shares * lastPrice : 0.0;
        const double costPrice = row.value("cost_price").toDouble();
        const double costValue = costPrice > 0.0 ? shares * costPrice : 0.0;
        const bool hasCost = costValue > 0.0;
        const double pnl = hasCost ? marketValue - costValue : 0.0;
        if (totalValue)
            *totalValue += marketValue;
        if (totalPnl && hasCost)
            *totalPnl += pnl;

        out.append(QVariantMap{
            {"symbol", displayCode(code)},
            {"name", row.value("name").toString()},
            {"shares", shares},
            {"costPrice", costPrice},
            {"lastPrice", lastPrice},
            {"marketValue", marketValue},
            {"pnl", pnl},
            {"pnlRate", hasCost ? pnl / costValue : 0.0},
            {"hasCost", hasCost},
            {"weight", 0.0},
            {"date", prettyDate(bar.date)},
        });
    }

    const double base = totalValue ? *totalValue : 0.0;
    if (base > 0.0) {
        for (QVariant& value : out) {
            QVariantMap row = value.toMap();
            row.insert("weight", row.value("marketValue").toDouble() / base);
            value = row;
        }
    }
    return out;
}

QVariantList EtfRotationController::buildAdvice(const QVariantList& rankings, const QHash<QString, Bar>& latestBars,
                                                double targetCashRatio, int holdNum) const {
    Q_UNUSED(holdNum);
    QVariantList out;
    const double realCash = std::max(0.0, portfolio_.value("availableCash").toDouble());
    const QVariantList positions = portfolio_.value("positions").toList();

    QHash<QString, QVariantMap> realByCode;
    QSet<QString> realSet;
    double holdingValue = 0.0;
    for (const QVariant& value : positions) {
        const QVariantMap row = value.toMap();
        const QString code = normalizeCode(row.value("code").toString());
        const double shares = row.value("shares").toDouble();
        if (shares <= 0.0)
            continue;
        realSet.insert(code);
        realByCode.insert(code, row);
        const double close = latestBars.value(code).close;
        if (close > 0.0)
            holdingValue += shares * close;
    }

    QStringList targets;
    QHash<QString, QVariantMap> rankByCode;
    for (const QVariant& value : rankings) {
        const QVariantMap row = value.toMap();
        const QString code = normalizeCode(row.value("symbol").toString());
        rankByCode.insert(code, row);
        if (row.value("selected").toBool())
            targets.append(code);
    }
    const QSet<QString> targetSet(targets.begin(), targets.end());

    double nonTargetSellProceeds = 0.0;
    for (const QString& code : realSet) {
        if (targetSet.contains(code))
            continue;
        const QVariantMap p = realByCode.value(code);
        const double shares = p.value("shares").toDouble();
        const double px = latestBars.value(code).close;
        const double amount = shares * px;
        nonTargetSellProceeds += proceedsAfterSell(shares, px);
        out.append(QVariantMap{{"action", "SELL"},
                               {"symbol", displayCode(code)},
                               {"name", p.value("name").toString()},
                               {"shares", shares},
                               {"price", px},
                               {"amount", amount},
                               {"reason", "不在最新目标组合，按原策略列入卖出候选"}});
    }

    if (targets.isEmpty()) {
        const double cashAfterSells = realCash + nonTargetSellProceeds;
        out.append(QVariantMap{{"action", "CASH"},
                               {"symbol", ""},
                               {"name", "保持现金"},
                               {"shares", 0},
                               {"price", 0},
                               {"amount", cashAfterSells},
                               {"reason", "当前没有标的通过筛选；金额为可用现金加预计卖出回款"}});
        return out;
    }

    QHash<QString, double> currentValueByCode;
    QHash<QString, double> currentSharesByCode;
    double targetCurrentValue = 0.0;
    for (const QString& code : targets) {
        const QVariantMap p = realByCode.value(code);
        const double shares = p.value("shares").toDouble();
        const double px = latestBars.value(code).close;
        const double value = shares > 0.0 && px > 0.0 ? shares * px : 0.0;
        currentSharesByCode.insert(code, shares);
        currentValueByCode.insert(code, value);
        targetCurrentValue += value;
    }

    const double cashAfterNonTargetSells = realCash + nonTargetSellProceeds;
    const double estimatedPortfolioValue = cashAfterNonTargetSells + targetCurrentValue;
    const double targetCashReserve = estimatedPortfolioValue * std::clamp(targetCashRatio, 0.0, 0.9);
    const double targetEquityValue = std::max(0.0, estimatedPortfolioValue - targetCashReserve);
    const double targetValuePerEtf = targets.isEmpty() ? 0.0 : targetEquityValue / targets.size();

    struct Need {
        QString code;
        double amount = 0.0;
    };
    QVector<Need> buyNeeds;
    double targetSellProceeds = 0.0;
    constexpr double lot = 100.0;
    for (const QString& code : targets) {
        const Bar bar = latestBars.value(code);
        const double px = bar.close;
        if (px <= 0.0 || !std::isfinite(px))
            continue;
        const double currentValue = currentValueByCode.value(code);
        const double currentShares = currentSharesByCode.value(code);
        const double delta = targetValuePerEtf - currentValue;
        const double minLotValue = px * lot;
        const QVariantMap rankRow = rankByCode.value(code);
        if (delta <= -minLotValue) {
            const double trimValue = -delta;
            const double sellShares = std::min(currentShares, std::floor(trimValue / minLotValue) * lot);
            const double amount = sellShares * px;
            if (sellShares > 0.0) {
                targetSellProceeds += proceedsAfterSell(sellShares, px);
                out.append(QVariantMap{{"action", "SELL"},
                                       {"symbol", displayCode(code)},
                                       {"name", rankRow.value("name").toString()},
                                       {"shares", sellShares},
                                       {"price", px},
                                       {"amount", amount},
                                       {"currentValue", currentValue},
                                       {"targetValue", targetValuePerEtf},
                                       {"reason", "仍在目标组合但当前市值高于等权目标，建议减至目标仓位附近"}});
                continue;
            }
        }
        if (delta >= minLotValue) {
            buyNeeds.append(Need{code, delta});
            continue;
        }
        out.append(QVariantMap{{"action", "HOLD"},
                               {"symbol", displayCode(code)},
                               {"name", rankRow.value("name").toString()},
                               {"shares", currentShares},
                               {"price", px},
                               {"amount", currentValue},
                               {"currentValue", currentValue},
                               {"targetValue", targetValuePerEtf},
                               {"reason", currentValue > 0.0 ? "接近等权目标，暂不需要调整" : "入选目标组合，但差额不足一手，暂不生成买入数量"}});
    }

    const double cashAvailableForBuys = std::max(0.0, cashAfterNonTargetSells + targetSellProceeds - targetCashReserve);
    double totalBuyNeed = 0.0;
    for (const Need& need : buyNeeds)
        totalBuyNeed += need.amount;
    const double buyScale = totalBuyNeed > 0.0 ? std::min(1.0, cashAvailableForBuys / totalBuyNeed) : 0.0;
    double plannedBuyAmount = 0.0;
    for (const Need& need : buyNeeds) {
        const QString code = need.code;
        const Bar bar = latestBars.value(code);
        const double budget = need.amount * buyScale;
        const double shares = bar.close > 0.0 ? std::floor(budget / (bar.close * 100.0)) * 100.0 : 0.0;
        const double suggestedCash = shares * bar.close;
        const QVariantMap rankRow = rankByCode.value(code);
        if (shares <= 0.0) {
            out.append(QVariantMap{{"action", "WAIT"},
                                   {"symbol", displayCode(code)},
                                   {"name", rankRow.value("name").toString()},
                                   {"shares", 0.0},
                                   {"price", bar.close},
                                   {"amount", budget},
                                   {"currentValue", currentValueByCode.value(code)},
                                   {"targetValue", targetValuePerEtf},
                                   {"reason", "入选目标组合且低于等权目标，但可用差额不足一手 100 份"}});
            continue;
        }
        plannedBuyAmount += suggestedCash;
        out.append(QVariantMap{{"action", "BUY"},
                               {"symbol", displayCode(code)},
                               {"name", rankRow.value("name").toString()},
                               {"shares", shares},
                               {"price", bar.close},
                               {"amount", suggestedCash},
                               {"currentValue", currentValueByCode.value(code)},
                               {"targetValue", targetValuePerEtf},
                               {"reason", currentValueByCode.value(code) > 0.0
                                              ? "仍在目标组合但低于等权目标，建议补到目标仓位附近"
                                              : "新入选目标组合，按等权目标仓位估算"}});
    }

    const double reservedCash = std::max(0.0, cashAfterNonTargetSells + targetSellProceeds - plannedBuyAmount);
    out.append(QVariantMap{{"action", "CASH"},
                           {"symbol", ""},
                           {"name", "预计交易后现金"},
                           {"shares", 0},
                           {"price", 0},
                           {"amount", reservedCash},
                           {"targetCash", targetCashReserve},
                           {"reason", QString("现金=当前可用现金+预计卖出回款-预计买入金额；目标现金约 %1")
                                          .arg(QString::number(targetCashReserve, 'f', 2))}});
    Q_UNUSED(holdingValue);
    return out;
}

QVariantList EtfRotationController::buildPricePoints(const QVariantList& trades) const {
    QVariantList points;
    for (const QVariant& value : trades) {
        const QVariantMap trade = value.toMap();
        const QString side = trade.value("side").toString();
        const double price = trade.value("price").toDouble();
        if (price <= 0.0 || side.isEmpty())
            continue;
        points.append(QVariantMap{
            {"symbol", trade.value("symbol").toString()},
            {"name", trade.value("name").toString()},
            {"date", trade.value("date").toString()},
            {"signalDate", trade.value("signalDate").toString()},
            {"side", side},
            {"price", price},
            {"shares", trade.value("shares").toDouble()},
            {"reason", trade.value("reason").toString()},
        });
    }
    return points;
}

QVariantList EtfRotationController::buildTradeCharts(const QHash<QString, QVector<Bar>>& byCode, const QVariantList& trades,
                                                     const QString& startDate, const QString& endDate) const {
    QHash<QString, QString> nameByCode;
    for (const QVariant& value : universe_) {
        const QVariantMap row = value.toMap();
        nameByCode.insert(normalizeCode(row.value("code").toString()), row.value("name").toString());
    }

    QHash<QString, QVariantList> tradesByCode;
    QSet<QString> wantedCodes;
    for (const QVariant& value : trades) {
        const QVariantMap trade = value.toMap();
        const QString code = normalizeCode(trade.value("symbol").toString());
        if (code.isEmpty())
            continue;
        QVariantList rows = tradesByCode.value(code);
        rows.append(trade);
        tradesByCode.insert(code, rows);
        wantedCodes.insert(code);
    }

    QStringList codes = wantedCodes.values();
    std::sort(codes.begin(), codes.end());
    QVariantList charts;
    for (const QString& code : codes) {
        QVector<Bar> series = byCode.value(code);
        if (series.isEmpty())
            continue;
        std::sort(series.begin(), series.end(), [](const Bar& a, const Bar& b) { return a.date < b.date; });
        QVariantList points;
        for (const Bar& bar : series) {
            if (bar.date < startDate || bar.date > endDate)
                continue;
            points.append(QVariantMap{{"date", prettyDate(bar.date)}, {"close", bar.close}});
        }
        if (points.isEmpty())
            continue;
        charts.append(QVariantMap{
            {"symbol", displayCode(code)},
            {"name", nameByCode.value(code, code)},
            {"lastClose", points.last().toMap().value("close").toDouble()},
            {"series", points},
            {"trades", tradesByCode.value(code)},
        });
    }
    return charts;
}

QVariantMap EtfRotationController::buildSummary(const QVariantMap& metrics, const QVariantList& rankings,
                                                const QVariantList& advice, const QVariantList& dataIssues) const {
    QStringList selected;
    for (const QVariant& value : rankings) {
        const QVariantMap row = value.toMap();
        if (row.value("selected").toBool())
            selected.append(row.value("name").toString() + " " + row.value("symbol").toString());
    }

    int buyCount = 0;
    int sellCount = 0;
    int holdCount = 0;
    int waitCount = 0;
    for (const QVariant& value : advice) {
        const QString action = value.toMap().value("action").toString();
        if (action == "BUY")
            ++buyCount;
        else if (action == "SELL")
            ++sellCount;
        else if (action == "HOLD")
            ++holdCount;
        else if (action == "WAIT")
            ++waitCount;
    }

    const bool blocked = !dataIssues.isEmpty();
    QString conclusion;
    if (blocked) {
        conclusion = "发现疑似复权/分红异常，已阻止继续回测";
    } else if (selected.isEmpty()) {
        conclusion = "当前没有标的通过筛选，策略偏向保留现金";
    } else {
        conclusion = QString("目标组合：%1").arg(selected.join("、"));
    }

    QVariantMap out;
    out["period"] = metrics.value("startDate").toString() + " / " + metrics.value("endDate").toString();
    out["conclusion"] = conclusion;
    out["selected"] = selected;
    out["buyCount"] = buyCount;
    out["sellCount"] = sellCount;
    out["holdCount"] = holdCount;
    out["waitCount"] = waitCount;
    out["dataQuality"] = blocked ? "blocked" : "ok";
    out["dataIssueCount"] = dataIssues.size();
    out["headline"] = blocked
                          ? "数据质量需要先修复"
                          : QString("累计 %1%，回撤 %2%")
                                .arg(QString::number(metrics.value("cumulativeReturn").toDouble() * 100.0, 'f', 2),
                                     QString::number(metrics.value("maxDrawdown").toDouble() * 100.0, 'f', 2));
    return out;
}

bool EtfRotationController::runDefault(bool updateData, const QString& startDateText, const QString& endDateText,
                                       int holdNum, double targetCashRatio, double stopLossRate, double realCash) {
    setRunning(true);
    ensureSeed();
    refresh();
    refreshPortfolio();
    const QString start = compactDate(startDateText).isEmpty() ? "20210101" : compactDate(startDateText);
    const QString end = compactDate(endDateText).isEmpty() ? previousBusinessDate() : compactDate(endDateText);
    const QString warmupStart = addDaysCompact(start, -kWarmupCalendarDays);
    auto clearResultState = [&]() {
        metrics_.clear();
        equity_.clear();
        trades_.clear();
        rankings_.clear();
        advice_.clear();
        pricePoints_.clear();
        tradeCharts_.clear();
        backtestHoldings_.clear();
        realHoldings_.clear();
        reportMarkdown_.clear();
        summary_.clear();
        summary_.insert("headline", dataIssues_.isEmpty() ? "运行未完成" : "数据质量需要先修复");
        summary_.insert("conclusion", statusMessage_);
        summary_.insert("buyCount", 0);
        summary_.insert("sellCount", 0);
        summary_.insert("holdCount", 0);
        summary_.insert("waitCount", 0);
        summary_.insert("dataQuality", dataIssues_.isEmpty() ? "empty" : "blocked");
        summary_.insert("dataIssueCount", dataIssues_.size());
        emit resultChanged();
    };

    QList<Bar> bars = loadBars();
    bool cacheHit = false;
    bool fetched = false;
    bool fetchFailed = false;
    if (updateData) {
        if (hasLocalDataCoverage(bars, warmupStart, end)) {
            cacheHit = true;
            setStatus(QString("缓存命中：本地 qfq 日线覆盖 %1 至 %2，跳过 AkShare 下载")
                          .arg(prettyDate(warmupStart), prettyDate(end)));
        } else {
            fetched = fetchLiveData(warmupStart, end);
            fetchFailed = !fetched;
            bars = loadBars();
            cacheHit = hasLocalDataCoverage(bars, warmupStart, end);
        }
    }
    dataIssues_ = collectDataIssues(bars);
    if (bars.isEmpty() || !dataIssues_.isEmpty()) {
        if (importLegacyBarsIfAvailable())
            bars = loadBars();
        dataIssues_ = collectDataIssues(bars);
    }
    if ((bars.isEmpty() || !dataIssues_.isEmpty()) && qEnvironmentVariableIsSet("MYQUANT_ALLOW_DEMO_DATA")) {
        generateDemoData(warmupStart, end);
        bars = loadBars();
        dataIssues_ = collectDataIssues(bars);
    }
    if (bars.isEmpty()) {
        setStatus("无本地 ETF qfq 日线：自动补齐未成功，请检查 AkShare/Python 设置或网络后再运行");
        clearResultState();
        setRunning(false);
        return false;
    }
    if (!dataIssues_.isEmpty()) {
        setStatus("ETF 日线数据异常，已阻止回测；请运行 AkShare 更新或修复数据");
        clearResultState();
        setRunning(false);
        return false;
    }

    const QVariantMap result = runBacktest(bars, start, end, std::max(1, holdNum),
                                           std::clamp(targetCashRatio, 0.0, 0.9),
                                           std::clamp(stopLossRate, 0.0, 0.5),
                                           std::max(1000.0, realCash));
    if (result.contains("error")) {
        setStatus(result.value("error").toString());
        dataIssues_.clear();
        clearResultState();
        setRunning(false);
        return false;
    }
    metrics_ = result.value("metrics").toMap();
    equity_ = result.value("equity").toList();
    trades_ = result.value("trades").toList();
    rankings_ = result.value("rankings").toList();
    advice_ = result.value("advice").toList();
    pricePoints_ = result.value("pricePoints").toList();
    tradeCharts_ = result.value("tradeCharts").toList();
    backtestHoldings_ = result.value("backtestHoldings").toList();
    realHoldings_ = result.value("realHoldings").toList();
    summary_ = buildSummary(metrics_, rankings_, advice_, dataIssues_);
    reportMarkdown_ = QString("# ETF 轮动复盘\n\n"
                              "- 区间：%1 至 %2\n"
                              "- 累计收益：%3%\n"
                              "- 最大回撤：%4%\n"
                              "- 调仓次数：%5\n"
                              "- 风险半仓：%6\n"
                              "- 固定止损：%7\n\n"
                              "本报告使用 AkShare/本地 qfq 日线和真实持仓快照生成，仅用于复盘。\n")
                          .arg(metrics_.value("startDate").toString(),
                               metrics_.value("endDate").toString(),
                               QString::number(metrics_.value("cumulativeReturn").toDouble() * 100.0, 'f', 2),
                               QString::number(metrics_.value("maxDrawdown").toDouble() * 100.0, 'f', 2),
                              QString::number(metrics_.value("rebalanceCount").toInt()),
                              QString::number(metrics_.value("riskHalfCount").toInt()),
                              QString::number(metrics_.value("stopCount").toInt()));
    const QString actualRange = metrics_.value("startDate").toString() + " 至 " + metrics_.value("endDate").toString();
    if (updateData && cacheHit && !fetched) {
        setStatus("ETF 轮动已完成；缓存命中，已从本地 qfq 日线截取 " + actualRange);
    } else if (updateData && fetched && cacheHit) {
        setStatus("ETF 轮动已完成；AkShare 数据已合并进本地缓存，并截取 " + actualRange);
    } else if (updateData && fetchFailed) {
        setStatus("ETF 轮动已完成；AkShare 更新未完成，已使用本地缓存截取 " + actualRange);
    } else {
        setStatus("ETF 轮动已完成；使用本地 qfq 日线截取 " + actualRange);
    }
    setRunning(false);
    emit resultChanged();
    return true;
}

bool EtfRotationController::openEtfFolder() {
    return QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::etf()));
}
