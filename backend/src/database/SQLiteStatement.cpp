#include "database/SQLiteStatement.hpp"
#include <utility>

namespace chronoplan::database {

SQLiteStatement::SQLiteStatement(sqlite3* db, const std::string& sql) : db_(db) {
    const int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()), &stmt_, nullptr);
    if (rc != SQLITE_OK) throwSqliteError(db_, rc, "prepare");
}

SQLiteStatement::~SQLiteStatement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

SQLiteStatement::SQLiteStatement(SQLiteStatement&& o) noexcept
    : db_(o.db_), stmt_(std::exchange(o.stmt_, nullptr)) {}

SQLiteStatement& SQLiteStatement::operator=(SQLiteStatement&& o) noexcept {
    if (this != &o) {
        if (stmt_) sqlite3_finalize(stmt_);
        db_ = o.db_;
        stmt_ = std::exchange(o.stmt_, nullptr);
    }
    return *this;
}

SQLiteStatement& SQLiteStatement::bindInt(int i, long long v) {
    const int rc = sqlite3_bind_int64(stmt_, i, v);
    if (rc != SQLITE_OK) throwSqliteError(db_, rc, "bind");
    return *this;
}

SQLiteStatement& SQLiteStatement::bindText(int i, const std::string& v) {
    const int rc = sqlite3_bind_text(stmt_, i, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) throwSqliteError(db_, rc, "bind");
    return *this;
}

SQLiteStatement& SQLiteStatement::bindNull(int i) {
    const int rc = sqlite3_bind_null(stmt_, i);
    if (rc != SQLITE_OK) throwSqliteError(db_, rc, "bind");
    return *this;
}

SQLiteStatement& SQLiteStatement::bindOptInt(int i, const std::optional<int>& v) {
    return v ? bindInt(i, *v) : bindNull(i);
}

SQLiteStatement& SQLiteStatement::bindOptText(int i, const std::optional<std::string>& v) {
    return v ? bindText(i, *v) : bindNull(i);
}

bool SQLiteStatement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throwSqliteError(db_, rc, "step");
}

void SQLiteStatement::execute() {
    while (step()) {}
}

void SQLiteStatement::reset() {
    sqlite3_reset(stmt_);          // its return value repeats the last step() error, already reported
    sqlite3_clear_bindings(stmt_);
}

bool SQLiteStatement::columnIsNull(int c) const { return sqlite3_column_type(stmt_, c) == SQLITE_NULL; }
long long SQLiteStatement::columnInt64(int c) const { return sqlite3_column_int64(stmt_, c); }
int SQLiteStatement::columnInt(int c) const { return sqlite3_column_int(stmt_, c); }

std::string SQLiteStatement::columnText(int c) const {
    const auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, c));
    return p ? std::string(p, static_cast<size_t>(sqlite3_column_bytes(stmt_, c))) : std::string();
}

std::optional<std::string> SQLiteStatement::columnOptText(int c) const {
    return columnIsNull(c) ? std::nullopt : std::optional<std::string>(columnText(c));
}

std::optional<int> SQLiteStatement::columnOptInt(int c) const {
    return columnIsNull(c) ? std::nullopt : std::optional<int>(columnInt(c));
}

}  // namespace chronoplan::database
