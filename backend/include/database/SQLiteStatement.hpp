#pragma once
// RAII prepared statement. Parameter indices are 1-based, column indices 0-based.
#include <cstddef>
#include <optional>
#include <string>
#include <sqlite3.h>
#include "database/Database.hpp"

namespace chronoplan::database {

class SQLiteStatement {
public:
    SQLiteStatement(sqlite3* db, const std::string& sql);  // throws DatabaseError
    ~SQLiteStatement();
    SQLiteStatement(SQLiteStatement&& other) noexcept;
    SQLiteStatement& operator=(SQLiteStatement&& other) noexcept;
    SQLiteStatement(const SQLiteStatement&) = delete;
    SQLiteStatement& operator=(const SQLiteStatement&) = delete;

    SQLiteStatement& bindInt(int index, long long v);
    SQLiteStatement& bindText(int index, const std::string& v);
    SQLiteStatement& bindNull(int index);
    SQLiteStatement& bindOptInt(int index, const std::optional<int>& v);
    SQLiteStatement& bindOptText(int index, const std::optional<std::string>& v);

    bool step();      // true = a row is available, false = finished. Throws DatabaseError.
    void execute();   // runs to completion, discarding rows
    void reset();     // reuse with new bindings

    bool columnIsNull(int col) const;
    long long columnInt64(int col) const;
    int columnInt(int col) const;
    std::string columnText(int col) const;
    std::optional<std::string> columnOptText(int col) const;
    std::optional<int> columnOptInt(int col) const;

private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace chronoplan::database
