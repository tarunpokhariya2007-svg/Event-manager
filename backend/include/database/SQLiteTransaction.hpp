#pragma once
#include <mutex>
#include "database/Database.hpp"

namespace chronoplan::database {

// RAII transaction. The outermost one is BEGIN IMMEDIATE (takes the write lock up
// front, so no lock-upgrade deadlocks); nested ones are SAVEPOINTs, so repository
// methods can be composed. Destruction without commit() rolls back.
// Holds the Database mutex for its lifetime.
class SQLiteTransaction {
public:
    explicit SQLiteTransaction(Database& db);
    ~SQLiteTransaction();
    SQLiteTransaction(const SQLiteTransaction&) = delete;
    SQLiteTransaction& operator=(const SQLiteTransaction&) = delete;

    void commit();     // throws DatabaseError; the transaction stays open (and rolls back) on failure
    void rollback();   // explicit rollback; safe to call once

private:
    Database& db_;
    std::unique_lock<std::recursive_mutex> lock_;
    int level_;        // 1 = outermost
    bool finished_ = false;
};

}  // namespace chronoplan::database
