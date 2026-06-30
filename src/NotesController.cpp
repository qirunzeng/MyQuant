#include "NotesController.h"

#include "AppPaths.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

namespace {
QString nowIso() {
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}

QString today() {
    return QDate::currentDate().toString("yyyy-MM-dd");
}

int wordCount(const QString& text) {
    return text.simplified().isEmpty() ? 0 : text.simplified().split(' ').size();
}

QString slug(QString title) {
    QString out = title.trimmed().toLower();
    out.replace(QRegularExpression("[^a-z0-9\\u4e00-\\u9fff]+"), "-");
    out = out.replace(QRegularExpression("^-+|-+$"), "");
    return out.isEmpty() ? "note" : out.left(80);
}
}

NotesController::NotesController(QObject* parent) : QObject(parent) {
    AppPaths::ensureAll();
    ensureDatabase();
    refresh();
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

    QSqlQuery q(db_);
    const bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS notes ("
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
        ")");
    if (!ok) {
        setStatus("笔记表初始化失败：" + q.lastError().text());
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
        setStatus("读取笔记失败：" + q.lastError().text());
        return;
    }
    while (q.next())
        notes_.append(rowToNote(q));
    setStatus(QString("已载入 %1 条复盘笔记").arg(notes_.size()));
    emit notesChanged();
}

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

    return {
        {"id", 0},
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
        {"wordCount", wordCount(content)},
    };
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
        setStatus("保存笔记失败：" + q.lastError().text());
        return false;
    }
    refresh();
    setStatus("复盘笔记已保存");
    return true;
}

bool NotesController::deleteNote(int id) {
    if (!ensureDatabase() || id <= 0)
        return false;
    QSqlQuery q(db_);
    q.prepare("DELETE FROM notes WHERE id=?");
    q.addBindValue(id);
    if (!q.exec()) {
        setStatus("删除笔记失败：" + q.lastError().text());
        return false;
    }
    refresh();
    setStatus("复盘笔记已删除");
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
        setStatus("找不到要导出的笔记");
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
