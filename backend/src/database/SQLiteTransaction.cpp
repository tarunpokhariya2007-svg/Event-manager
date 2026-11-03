#include "database/SQLiteTransaction.hpp"
#include <string>

namespace chronoplan::database {

namespace {
std::string savepointName(int level) { return "chrono_sp_" + std::to_string(level); }
}

SQLiteTransaction::SQLiteTransaction(Database& db)
    : db_(db), lock_(db.lock()), level_(db.transactionDepth() + 1) {
    if (level_ == 1) db_.exec("BEGIN IMMEDIATE");
    else db_.exec("SAVEPOINT " + savepointName(level_));
    db_.setTransactionDepth(level_);
}

SQLiteTransaction::~SQLiteTransaction() {
    if (finished_) return;
    try { rollback(); } catch (...) {}   // destructors must not throw
}

void SQLiteTransaction::commit() {
    if (finished_) throw DatabaseError("transaction already finished");
    if (level_ == 1) db_.exec("COMMIT");
    else db_.exec("RELEASE " + savepointName(level_));
    finished_ = true;
    db_.setTransactionDepth(level_ - 1);
}

void SQLiteTransaction::rollback() {
    if (finished_) return;
    finished_ = true;
    db_.setTransactionDepth(level_ - 1);
    if (level_ == 1) {
        db_.exec("ROLLBACK");
    } else {
        db_.exec("ROLLBACK TO " + savepointName(level_));
        db_.exec("RELEASE " + savepointName(level_));
    }
}

}  // namespace chronoplan::database
