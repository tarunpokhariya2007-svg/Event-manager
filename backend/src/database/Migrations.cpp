#include "database/Migrations.hpp"
#include "database/SQLiteStatement.hpp"
#include "database/SQLiteTransaction.hpp"
#include "MigrationsData.hpp"

namespace chronoplan::database {

const std::vector<Migration>& allMigrations() {
    static const std::vector<Migration> migrations = {
        {1, "initial schema", generated::kMigration001},
        {2, "accounts, sessions and per-user data", generated::kMigration002},
    };
    return migrations;
}

int latestSchemaVersion() { return allMigrations().back().version; }

int currentSchemaVersion(Database& db) {
    auto lock = db.lock();
    SQLiteStatement s = db.prepare("PRAGMA user_version");
    return s.step() ? s.columnInt(0) : 0;
}

void applyMigrations(Database& db) {
    auto lock = db.lock();
    if (currentSchemaVersion(db) > latestSchemaVersion())
        throw DatabaseError("database schema is newer than this version of ChronoPlan supports");

    for (const Migration& m : allMigrations()) {
        SQLiteTransaction tx(db);
        // Re-check inside the write lock: another process may have migrated meanwhile.
        if (currentSchemaVersion(db) >= m.version) continue;
        db.exec(m.sql);
        db.exec("PRAGMA user_version = " + std::to_string(m.version));  // integer literal, not user input
        tx.commit();
    }
}

}  // namespace chronoplan::database
