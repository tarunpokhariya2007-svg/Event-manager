#pragma once
// Test helpers: every test gets its own temporary SQLite file (never the real DB).
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>
#include "database/DatabaseManager.hpp"
#include "database/SQLiteStatement.hpp"
#include "models/Event.hpp"
#include "models/Task.hpp"

namespace testutil {

class TempDb {
public:
    TempDb() {
        static std::atomic<int> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        dir_ = std::filesystem::temp_directory_path() /
               ("chronoplan_test_" + std::to_string(stamp) + "_" + std::to_string(counter++));
        path_ = (dir_ / "nested" / "test.db").string();   // also exercises directory creation
    }
    ~TempDb() { std::error_code ec; std::filesystem::remove_all(dir_, ec); }
    chronoplan::database::DatabaseConfig config() const {
        chronoplan::database::DatabaseConfig c;
        c.path = path_;
        return c;
    }
    const std::string& path() const { return path_; }
private:
    std::filesystem::path dir_;
    std::string path_;
};

// Inserts a password-less test account and returns its id (repositories are bound to a user).
inline long long makeUser(chronoplan::database::Database& db, const std::string& email = "user@example.com") {
    auto lock = db.lock();
    chronoplan::database::SQLiteStatement s = db.prepare(
        "INSERT INTO users (email, password_hash, created_at) VALUES (?, 'not-a-real-hash', 0)");
    s.bindText(1, email);
    s.execute();
    return static_cast<long long>(sqlite3_last_insert_rowid(db.handle()));
}

inline chronoplan::models::Id num(long long v) {
    chronoplan::models::Id id; id.value = std::to_string(v); id.numeric = true; return id;
}
inline chronoplan::models::Id str(const std::string& v) {
    chronoplan::models::Id id; id.value = v; id.numeric = false; return id;
}

// Task from JSON (so defaults/validation match the API). `id` may be json(nullptr)/absent.
inline chronoplan::models::Task makeTask(const nlohmann::json& id, const std::string& title = "Task",
                                         nlohmann::json extra = nlohmann::json::object()) {
    nlohmann::json j = {{"title", title}, {"duration_min", 60}};
    if (!id.is_null()) j["id"] = id;
    for (auto it = extra.begin(); it != extra.end(); ++it) j[it.key()] = it.value();
    return chronoplan::models::Task::fromJson(j);
}

inline chronoplan::models::Event makeEvent(const nlohmann::json& id, const std::string& date = "2026-11-02",
                                           int start = 540, int end = 600,
                                           nlohmann::json extra = nlohmann::json::object()) {
    nlohmann::json j = {{"title", "Meeting"}, {"date", date}, {"start_min", start}, {"end_min", end}};
    if (!id.is_null()) j["id"] = id;
    for (auto it = extra.begin(); it != extra.end(); ++it) j[it.key()] = it.value();
    return chronoplan::models::Event::fromJson(j);
}

}  // namespace testutil
