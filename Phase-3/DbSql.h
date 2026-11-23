#pragma once
// Every SQL statement the game uses, in one place and free of Qt, so the very same text can be tested against a real
// SQLite engine (tools/test_schema.py reads this file). Values are always passed as bound parameters (the ? marks):
// text typed by a player is never pasted into SQL, so it cannot change what a statement does.

inline constexpr const char* kSqlPragmaForeignKeys = "PRAGMA foreign_keys = ON";

inline constexpr const char* kSqlCreateUsers = R"SQL(
CREATE TABLE IF NOT EXISTS users (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  username      TEXT NOT NULL UNIQUE,
  salt          TEXT NOT NULL,
  password_hash TEXT NOT NULL,
  created_at    TEXT NOT NULL DEFAULT (datetime('now'))
)
)SQL";

inline constexpr const char* kSqlCreateSaves = R"SQL(
CREATE TABLE IF NOT EXISTS saves (
  user_id     INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  case_id     TEXT NOT NULL,
  location    TEXT NOT NULL,
  credibility INTEGER NOT NULL,
  player_x    REAL NOT NULL,
  player_y    REAL NOT NULL,
  saved_at    TEXT NOT NULL DEFAULT (datetime('now')),
  PRIMARY KEY (user_id, case_id)
)
)SQL";

inline constexpr const char* kSqlCreateItems = R"SQL(
CREATE TABLE IF NOT EXISTS save_items (
  user_id INTEGER NOT NULL,
  case_id TEXT NOT NULL,
  kind    TEXT NOT NULL CHECK (kind IN ('clue', 'exposed', 'pressed')),
  value   TEXT NOT NULL,
  seq     INTEGER NOT NULL,
  FOREIGN KEY (user_id, case_id) REFERENCES saves(user_id, case_id) ON DELETE CASCADE
)
)SQL";

inline constexpr const char* kSqlInsertUser = "INSERT INTO users (username, salt, password_hash) VALUES (?, ?, ?)";
inline constexpr const char* kSqlSelectUser = "SELECT id, salt, password_hash FROM users WHERE username = ?";

inline constexpr const char* kSqlDeleteItems = "DELETE FROM save_items WHERE user_id = ? AND case_id = ?";
inline constexpr const char* kSqlDeleteSave = "DELETE FROM saves WHERE user_id = ? AND case_id = ?";
inline constexpr const char* kSqlInsertSave =
    "INSERT INTO saves (user_id, case_id, location, credibility, player_x, player_y) VALUES (?, ?, ?, ?, ?, ?)";
inline constexpr const char* kSqlInsertItem =
    "INSERT INTO save_items (user_id, case_id, kind, value, seq) VALUES (?, ?, ?, ?, ?)";

inline constexpr const char* kSqlSelectSave =
    "SELECT location, credibility, player_x, player_y FROM saves WHERE user_id = ? AND case_id = ?";
inline constexpr const char* kSqlSelectItems =
    "SELECT kind, value FROM save_items WHERE user_id = ? AND case_id = ? ORDER BY seq";
inline constexpr const char* kSqlHasSave = "SELECT 1 FROM saves WHERE user_id = ? AND case_id = ?";
