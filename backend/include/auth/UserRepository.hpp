#pragma once
#include <optional>
#include <string>
#include "database/Database.hpp"
#include "repositories/UserId.hpp"

namespace chronoplan::auth {

struct User {
    repositories::UserId id = 0;
    std::string email;                          // lower-case
    std::optional<std::string> passwordHash;    // Argon2id; empty for Google-only accounts
    std::optional<std::string> googleSub;
    std::string displayName;
    bool emailVerified = false;
    bool isSystem = false;                      // the locked owner of pre-authentication data
    long long createdAt = 0;
};

class UserRepository {
public:
    explicit UserRepository(database::Database& db) : db_(db) {}

    // Throws auth::EmailTakenError when the e-mail (case-insensitive) or Google subject exists.
    User createPasswordUser(const std::string& email, const std::string& passwordHash,
                            const std::string& displayName, long long now);
    User createGoogleUser(const std::string& email, const std::string& googleSub,
                          const std::string& displayName, long long now);

    std::optional<User> findByEmail(const std::string& email);
    std::optional<User> findById(repositories::UserId id);
    std::optional<User> findByGoogleSub(const std::string& sub);

    void recordLogin(repositories::UserId id, long long now);
    void updatePasswordHash(repositories::UserId id, const std::string& passwordHash);

private:
    database::Database& db_;
};

}  // namespace chronoplan::auth
