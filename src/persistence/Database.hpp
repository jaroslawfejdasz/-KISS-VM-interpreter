#pragma once
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include "../vendor/sqlite3/sqlite3.h"

namespace minima::persistence {

/**
 * RAII wrapper around SQLite3 connection.
 * Thread-safety: single-threaded (one connection per instance).
 */
class Database {
public:
    explicit Database(const std::string& path) {
        int rc = sqlite3_open(path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::string err = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("Database::open failed: " + err);
        }
        // Performance pragmas
        exec("PRAGMA journal_mode=WAL;");
        exec("PRAGMA synchronous=NORMAL;");
        exec("PRAGMA temp_store=MEMORY;");
        exec("PRAGMA mmap_size=268435456;"); // 256MB mmap
    }

    ~Database() {
        if (db_) sqlite3_close(db_);
    }

    // Non-copyable, movable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Database(Database&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }
    Database& operator=(Database&& o) noexcept {
        if (this != &o) { if (db_) sqlite3_close(db_); db_ = o.db_; o.db_ = nullptr; }
        return *this;
    }

    /** Execute a statement with no result rows. */
    void exec(const std::string& sql) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "unknown error";
            sqlite3_free(errmsg);
            throw std::runtime_error("Database::exec failed [" + sql.substr(0,60) + "]: " + msg);
        }
    }

    /** Prepare a statement for reuse. */
    sqlite3_stmt* prepare(const std::string& sql) {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
            throw std::runtime_error("Database::prepare failed: " + std::string(sqlite3_errmsg(db_)));
        return stmt;
    }

    /** Run a callback for each row returned by stmt. */
    void step(sqlite3_stmt* stmt, std::function<void(sqlite3_stmt*)> callback) {
        int rc;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            callback(stmt);
        }
        if (rc != SQLITE_DONE)
            throw std::runtime_error("Database::step error: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt);
    }

    /** Execute single-step (INSERT/UPDATE/DELETE), no rows expected. */
    void stepOnce(sqlite3_stmt* stmt) {
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW)
            throw std::runtime_error("Database::stepOnce error: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt);
    }

    int64_t lastInsertRowid() const { return sqlite3_last_insert_rowid(db_); }

    sqlite3* handle() { return db_; }

    void beginTransaction() { exec("BEGIN;"); }
    void commit()           { exec("COMMIT;"); }
    void rollback()         { exec("ROLLBACK;"); }

private:
    sqlite3* db_ = nullptr;
};

} // namespace minima::persistence
