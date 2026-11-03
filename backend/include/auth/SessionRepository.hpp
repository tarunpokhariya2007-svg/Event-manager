#pragma once
#include <optional>
#include <string>
#include "database/Database.hpp"
#include "repositories/UserId.hpp"

namespace chronoplan::auth {

struct Session {
    long long id = 0;
    repositories::UserId userId = 0;
    std::string tokenHash;      // hex SHA-256 of the cookie token
    long long createdAt = 0;
    long long lastSeenAt = 0;
    long long expiresAt = 0;
};

class SessionRepository {
public:
    explicit SessionRepository(database::Database& db) : db_(db) {}

    void create(repositories::UserId userId, const std::string& tokenHash, long long now, long long expiresAt);
    std::optional<Session> findByTokenHash(const std::string& tokenHash);
    void touch(long long sessionId, long long now);
    void deleteByTokenHash(const std::string& tokenHash);
    void deleteAllForUser(repositories::UserId userId);
    // Removes sessions past their absolute expiry or idle for longer than the idle limit.
    void deleteStale(long long now, long long idleSeconds);

private:
    database::Database& db_;
};

}  // namespace chronoplan::auth
