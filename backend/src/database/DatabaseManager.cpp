#include "database/DatabaseManager.hpp"
#include <filesystem>
#include <iostream>
#include "database/Migrations.hpp"
#include "database/SQLiteStatement.hpp"

namespace chronoplan::database {

namespace {
bool isInMemory(const std::string& p) {
    return p == ":memory:" || p.rfind("file:", 0) == 0;   // URI/memory forms need no directory
}
}

DatabaseManager::DatabaseManager(const DatabaseConfig& config) {
    if (!isInMemory(config.path)) {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path parent = fs::path(config.path).parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent, ec);
            if (ec) {
                std::cerr << "[db] cannot create directory '" << parent.string() << "': " << ec.message() << '\n';
                throw DatabaseError("cannot create the database directory");
            }
        }
    }

    db_ = std::make_unique<Database>(config);
    applyMigrations(*db_);

    // Readiness check: schema at the expected version and tables queryable.
    if (schemaVersion() != latestSchemaVersion())
        throw DatabaseError("database schema version mismatch after migration");
    for (const char* sql : {"SELECT COUNT(*) FROM tasks", "SELECT COUNT(*) FROM events",
                            "SELECT COUNT(*) FROM task_dependencies", "SELECT COUNT(*) FROM users",
                            "SELECT COUNT(*) FROM sessions"}) {
        auto lock = db_->lock();
        SQLiteStatement s = db_->prepare(sql);
        s.step();
    }
}

int DatabaseManager::schemaVersion() { return currentSchemaVersion(*db_); }

}  // namespace chronoplan::database
