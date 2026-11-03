#pragma once
#include <vector>
#include "database/Database.hpp"

namespace chronoplan::database {

struct Migration {
    int version;           // strictly increasing, starting at 1
    const char* name;
    const char* sql;       // may contain several statements
};

// Ordered list, sourced from backend/db/migrations/*.sql (embedded at build time).
const std::vector<Migration>& allMigrations();
int latestSchemaVersion();

int currentSchemaVersion(Database& db);          // PRAGMA user_version
void applyMigrations(Database& db);              // idempotent; each step is atomic

}  // namespace chronoplan::database
