#include "Database.h"
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>
#include "DbSql.h"

namespace {
QString qs(const std::string& s) { return QString::fromStdString(s); }
QString sql(const char* text) { return QString::fromUtf8(text); }
}  // namespace

QString Database::defaultPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/dossier.db";
}

bool Database::exec(const char* text) {
    QSqlQuery q(db_);
    if (!q.exec(sql(text))) { error_ = q.lastError().text(); return false; }
    return true;
}

bool Database::open(const QString& path) {
    close();
    db_ = QSqlDatabase::addDatabase("QSQLITE", name_);
    db_.setDatabaseName(path);
    if (!db_.open()) { error_ = db_.lastError().text(); return false; }
    return exec(kSqlPragmaForeignKeys) && exec(kSqlCreateUsers) && exec(kSqlCreateSaves) && exec(kSqlCreateItems);
}

void Database::close() {
    if (db_.isOpen()) db_.close();
    db_ = QSqlDatabase();   // let go of the connection before removing it
    if (QSqlDatabase::contains(name_)) QSqlDatabase::removeDatabase(name_);
}

std::optional<int> Database::createUser(const std::string& username, const std::string& salt, const std::string& hash) {
    QSqlQuery q(db_);
    q.prepare(sql(kSqlInsertUser));
    q.addBindValue(qs(username));
    q.addBindValue(qs(salt));
    q.addBindValue(qs(hash));
    if (!q.exec()) { error_ = q.lastError().text(); return std::nullopt; }   // for example: the name is taken
    return q.lastInsertId().toInt();
}

std::optional<UserRecord> Database::findUser(const std::string& username) {
    QSqlQuery q(db_);
    q.prepare(sql(kSqlSelectUser));
    q.addBindValue(qs(username));
    if (!q.exec()) { error_ = q.lastError().text(); return std::nullopt; }
    if (!q.next()) return std::nullopt;
    UserRecord u;
    u.id = q.value(0).toInt();
    u.salt = q.value(1).toString().toStdString();
    u.hash = q.value(2).toString().toStdString();
    return u;
}

bool Database::save(int userId, const std::string& caseId, const Snapshot& s, float x, float y) {
    if (!db_.transaction()) { error_ = db_.lastError().text(); return false; }

    // Run one statement with the given bound values.
    auto run = [this](const char* text, const QVariantList& values) {
        QSqlQuery q(db_);
        q.prepare(sql(text));
        for (const QVariant& v : values) q.addBindValue(v);
        if (!q.exec()) { error_ = q.lastError().text(); return false; }
        return true;
    };
    const QVariant uid = userId;
    const QVariant cid = qs(caseId);

    bool ok = run(kSqlDeleteItems, {uid, cid}) && run(kSqlDeleteSave, {uid, cid}) &&
              run(kSqlInsertSave, {uid, cid, qs(s.location), s.credibility, static_cast<double>(x), static_cast<double>(y)});
    int seq = 0;
    for (const Id& clue : s.found) ok = ok && run(kSqlInsertItem, {uid, cid, QStringLiteral("clue"), qs(clue), seq++});
    seq = 0;
    for (const std::string& key : s.exposed) ok = ok && run(kSqlInsertItem, {uid, cid, QStringLiteral("exposed"), qs(key), seq++});
    seq = 0;
    for (const std::string& key : s.pressed) ok = ok && run(kSqlInsertItem, {uid, cid, QStringLiteral("pressed"), qs(key), seq++});

    if (!ok) { db_.rollback(); return false; }          // nothing is half-saved
    if (!db_.commit()) { error_ = db_.lastError().text(); db_.rollback(); return false; }
    return true;
}

std::optional<SavedGame> Database::load(int userId, const std::string& caseId) {
    SavedGame g;
    {
        QSqlQuery q(db_);
        q.prepare(sql(kSqlSelectSave));
        q.addBindValue(userId);
        q.addBindValue(qs(caseId));
        if (!q.exec()) { error_ = q.lastError().text(); return std::nullopt; }
        if (!q.next()) return std::nullopt;
        g.snapshot.location = q.value(0).toString().toStdString();
        g.snapshot.credibility = q.value(1).toInt();
        g.x = static_cast<float>(q.value(2).toDouble());
        g.y = static_cast<float>(q.value(3).toDouble());
    }
    QSqlQuery q(db_);
    q.prepare(sql(kSqlSelectItems));
    q.addBindValue(userId);
    q.addBindValue(qs(caseId));
    if (!q.exec()) { error_ = q.lastError().text(); return std::nullopt; }
    while (q.next()) {
        const std::string kind = q.value(0).toString().toStdString();
        const std::string value = q.value(1).toString().toStdString();
        if (kind == "clue") g.snapshot.found.push_back(value);          // ORDER BY seq keeps the dossier order
        else if (kind == "exposed") g.snapshot.exposed.insert(value);
        else if (kind == "pressed") g.snapshot.pressed.insert(value);
    }
    return g;
}

bool Database::hasSave(int userId, const std::string& caseId) {
    QSqlQuery q(db_);
    q.prepare(sql(kSqlHasSave));
    q.addBindValue(userId);
    q.addBindValue(qs(caseId));
    if (!q.exec()) { error_ = q.lastError().text(); return false; }
    return q.next();
}

bool Database::deleteSave(int userId, const std::string& caseId) {
    if (!db_.transaction()) { error_ = db_.lastError().text(); return false; }
    bool ok = true;
    for (const char* text : {kSqlDeleteItems, kSqlDeleteSave}) {
        QSqlQuery q(db_);
        q.prepare(sql(text));
        q.addBindValue(userId);
        q.addBindValue(qs(caseId));
        if (!q.exec()) { error_ = q.lastError().text(); ok = false; break; }
    }
    if (!ok) { db_.rollback(); return false; }
    if (!db_.commit()) { error_ = db_.lastError().text(); db_.rollback(); return false; }
    return true;
}
