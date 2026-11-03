#pragma once
// SQLite connection (RAII). One connection is shared by all repositories and
// guarded by a recursive mutex, so Crow's worker threads never interleave
// statements or transactions on it.
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sqlite3.h>

namespace chronoplan::database {

// Public message is always generic; the real SQLite text goes to stderr only.
class DatabaseError : public std::runtime_error {
public:
    explicit DatabaseError(const std::string& message, int sqliteCode = 0)
        : std::runtime_error(message), code_(sqliteCode) {}
    int code() const noexcept { return code_; }
    bool isConstraint() const noexcept { return (code_ & 0xFF) == SQLITE_CONSTRAINT; }
    // Busy/locked/cannot-open/IO/read-only/full/corrupt: the service cannot use the database now.
    bool isUnavailable() const noexcept {
        switch (code_ & 0xFF) {
            case SQLITE_BUSY: case SQLITE_LOCKED: case SQLITE_CANTOPEN: case SQLITE_IOERR:
            case SQLITE_READONLY: case SQLITE_FULL: case SQLITE_CORRUPT: case SQLITE_NOTADB:
                return true;
            default: return false;
        }
    }
private:
    int code_;
};

struct DatabaseConfig {
    std::string path = "./data/chronoplan.db";  // ":memory:" is allowed (tests)
    int busyTimeoutMs = 5000;
    bool enableWal = true;   // WAL: readers don't block the writer; ignored for :memory:

    // CHRONOPLAN_DB_PATH, CHRONOPLAN_DB_BUSY_TIMEOUT_MS. Throws DatabaseError on a bad value.
    static DatabaseConfig fromEnvironment();
};

// Logs the real SQLite message (not for constraint errors) and throws DatabaseError.
[[noreturn]] void throwSqliteError(sqlite3* db, int rc, std::string_view context);

class SQLiteStatement;

class Database {
public:
    explicit Database(const DatabaseConfig& config);  // opens + applies PRAGMAs; throws DatabaseError
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() const noexcept { return db_; }
    const std::string& path() const noexcept { return path_; }

    // Hold this for the duration of any sequence of statements.
    std::unique_lock<std::recursive_mutex> lock() const { return std::unique_lock(mutex_); }

    void exec(const std::string& sql);                 // parameterless SQL only
    SQLiteStatement prepare(const std::string& sql);   // parameterised SQL
    long long changes() const noexcept { return sqlite3_changes(db_); }

    // Managed by SQLiteTransaction (0 = no open transaction).
    int transactionDepth() const noexcept { return depth_; }
    void setTransactionDepth(int d) noexcept { depth_ = d; }

private:
    sqlite3* db_ = nullptr;
    std::string path_;
    mutable std::recursive_mutex mutex_;
    int depth_ = 0;
};

}  // namespace chronoplan::database
