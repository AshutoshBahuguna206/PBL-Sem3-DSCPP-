#pragma once
// The game's database (SQLite through Qt's QtSql module). It stores users and saved games. It converts between the
// engine's plain Snapshot and rows in the tables, so the engine itself never sees SQL.
#include <QSqlDatabase>
#include <QString>
#include <optional>
#include <string>
#include "../Game.h"

struct UserRecord { int id = 0; std::string salt, hash; };
struct SavedGame { Snapshot snapshot; float x = 0.f, y = 0.f; };   // the state plus the player's position

class Database {
public:
    explicit Database(const QString& connectionName = "dossier") : name_(connectionName) {}
    ~Database() { close(); }
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Where the database file lives: the user's application-data folder (created if needed).
    static QString defaultPath();

    // Opens (and creates, if needed) the file and the tables. Returns false and sets lastError() on failure.
    bool open(const QString& path);
    void close();
    bool isOpen() const { return db_.isOpen(); }
    const QString& lastError() const { return error_; }

    // Users. createUser returns the new id, or nothing if the name is already taken.
    std::optional<int> createUser(const std::string& username, const std::string& salt, const std::string& hash);
    std::optional<UserRecord> findUser(const std::string& username);

    // Saved games, one per user and case. save() replaces any earlier save, all in one transaction.
    bool save(int userId, const std::string& caseId, const Snapshot& s, float x, float y);
    std::optional<SavedGame> load(int userId, const std::string& caseId);
    bool hasSave(int userId, const std::string& caseId);
    bool deleteSave(int userId, const std::string& caseId);   // true even if there was nothing to delete

private:
    bool exec(const char* sql);

    QString name_;
    QSqlDatabase db_;
    QString error_;
};
