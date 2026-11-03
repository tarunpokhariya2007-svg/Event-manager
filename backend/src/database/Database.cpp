#include "database/Database.hpp"
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include "database/SQLiteStatement.hpp"

namespace chronoplan::database {

void throwSqliteError(sqlite3* db, int rc, std::string_view context) {
    const DatabaseError probe("", rc);
    if (!probe.isConstraint()) {
        std::cerr << "[sqlite] " << context << ": rc=" << rc << " "
                  << (db ? sqlite3_errmsg(db) : sqlite3_errstr(rc)) << '\n';
    }
    throw DatabaseError("database operation failed", rc);
}

DatabaseConfig DatabaseConfig::fromEnvironment() {
    DatabaseConfig cfg;
    if (const char* p = std::getenv("CHRONOPLAN_DB_PATH"); p && *p) cfg.path = p;
    if (const char* t = std::getenv("CHRONOPLAN_DB_BUSY_TIMEOUT_MS"); t && *t) {
        char* end = nullptr;
        errno = 0;
        const long v = std::strtol(t, &end, 10);
        if (errno != 0 || *end != '\0' || v < 0 || v > 600000)
            throw DatabaseError("CHRONOPLAN_DB_BUSY_TIMEOUT_MS must be an integer 0..600000");
        cfg.busyTimeoutMs = static_cast<int>(v);
    }
    return cfg;
}

Database::Database(const DatabaseConfig& config) : path_(config.path) {
    if (path_.empty()) throw DatabaseError("database path is empty");
    const int rc = sqlite3_open_v2(path_.c_str(), &db_,
                                   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                                   nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[sqlite] cannot open database: " << (db_ ? sqlite3_errmsg(db_) : sqlite3_errstr(rc)) << '\n';
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw DatabaseError("cannot open the database", rc);
    }
    try {
        sqlite3_extended_result_codes(db_, 1);
        sqlite3_busy_timeout(db_, config.busyTimeoutMs);
        // Foreign keys are OFF by default in SQLite and are a per-connection setting.
        exec("PRAGMA foreign_keys = ON");
        {
            SQLiteStatement s = prepare("PRAGMA foreign_keys");
            if (!s.step() || s.columnInt(0) != 1)
                throw DatabaseError("SQLite foreign key enforcement is unavailable");
        }
        if (config.enableWal && path_ != ":memory:" && path_.rfind("file::memory:", 0) != 0) {
            exec("PRAGMA journal_mode = WAL");    // persistent, safe for one writer + many readers
            exec("PRAGMA synchronous = NORMAL");  // durable across app crashes; WAL-recommended
        }
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    if (rc != SQLITE_OK) throwSqliteError(db_, rc, "exec");
}

SQLiteStatement Database::prepare(const std::string& sql) { return SQLiteStatement(db_, sql); }

}  // namespace chronoplan::database
