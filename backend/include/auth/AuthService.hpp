#pragma once
// Orchestrates registration, sign-in (password and Google), session lifecycle and CSRF checks.
// It never sees HTTP; the Crow layer (api/AuthMiddleware, api/AuthRoutes) translates.
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include "auth/AuthConfig.hpp"
#include "auth/GoogleTokenVerifier.hpp"
#include "auth/RateLimiter.hpp"
#include "auth/SessionRepository.hpp"
#include "auth/UserRepository.hpp"
#include "database/Database.hpp"

namespace chronoplan::auth {

struct AuthenticatedUser {
    repositories::UserId id = 0;
    std::string email;
    std::string displayName;
    bool hasPassword = false;
    bool hasGoogle = false;
    std::string csrfToken;    // derived from the session token; the browser echoes it in X-CSRF-Token
};

struct SignInResult {
    AuthenticatedUser user;
    std::string sessionToken;     // goes into the HttpOnly cookie; never stored in plaintext
    long long maxAgeSeconds = 0;
    bool created = false;         // a new account was made (registration / first Google sign-in)
};

class AuthService {
public:
    using Clock = std::function<long long()>;   // unix seconds

    AuthService(database::Database& db, AuthConfig config, std::shared_ptr<IGoogleTokenVerifier> google, Clock clock);

    // `clientKey` identifies the caller for throttling (usually the remote IP address).
    SignInResult registerUser(const std::string& email, const std::string& password,
                              const std::string& displayName, const std::string& clientKey);
    SignInResult login(const std::string& email, const std::string& password, const std::string& clientKey);
    SignInResult loginWithGoogle(const std::string& idToken, const std::string& clientKey);

    // nullopt for a missing, unknown, expired or idle session (expired ones are deleted).
    std::optional<AuthenticatedUser> authenticate(const std::string& sessionToken);
    // Constant-time check of the X-CSRF-Token header value against the session's CSRF token.
    static bool csrfMatches(const AuthenticatedUser& user, const std::string& headerValue);
    void logout(const std::string& sessionToken);

    // The CSRF token that belongs to a session token (one-way derived; see Crypto).
    static std::string deriveCsrfToken(const std::string& sessionToken);

    const AuthConfig& config() const { return config_; }
    bool googleEnabled() const { return google_ != nullptr && !config_.googleClientId.empty(); }

private:
    SignInResult issueSession(const User& user, bool created);
    static AuthenticatedUser toAuthenticated(const User& user, const std::string& sessionToken);
    void requireNotThrottled(const std::string& emailKey, const std::string& clientKey);

    database::Database& db_;
    AuthConfig config_;
    std::shared_ptr<IGoogleTokenVerifier> google_;
    Clock clock_;
    UserRepository users_;
    SessionRepository sessions_;
    RateLimiter failedLogins_;
    RateLimiter registrations_;
};

}  // namespace chronoplan::auth
