#pragma once
#include <memory>
#include "database/Database.hpp"

namespace chronoplan::database {

// Startup entry point: creates the directory, opens the database, migrates the
// schema and verifies readiness. Throws DatabaseError (generic message) on failure.
class DatabaseManager {
public:
    explicit DatabaseManager(const DatabaseConfig& config);
    Database& database() noexcept { return *db_; }
    int schemaVersion();

private:
    std::unique_ptr<Database> db_;
};

}  // namespace chronoplan::database
