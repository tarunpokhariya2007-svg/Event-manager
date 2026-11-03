#pragma once
// Authentication settings read from the environment (12-factor), validated at startup.
#include <functional>
#include <string>

namespace chronoplan::auth {

struct AuthConfig {
    bool production = false;                 // CHRONOPLAN_ENV=production
    std::string frontendOrigin;              // CHRONOPLAN_CORS_ORIGIN, exact origin (never "*")
    std::string googleClientId;              // GOOGLE_CLIENT_ID (empty = Google sign-in disabled)
    bool cookieSecure = false;               // CHRONOPLAN_COOKIE_SECURE (default: true in production)
    std::string cookieSameSite = "Lax";      // CHRONOPLAN_COOKIE_SAMESITE: Lax | Strict | None
    std::string cookieName = "chronoplan_session";
    long long sessionTtlSeconds = 14LL * 24 * 3600;     // absolute lifetime
    long long sessionIdleSeconds = 7LL * 24 * 3600;     // sign out after this long without use
    bool trustForwardedFor = false;          // CHRONOPLAN_TRUST_X_FORWARDED_FOR: behind ONE trusted reverse proxy
    int maxFailedLogins = 10;                // per e-mail and per client address ...
    int failedLoginWindowSeconds = 15 * 60;  // ... within this window

    using Lookup = std::function<const char*(const char*)>;
    // Throws std::invalid_argument for unusable values, e.g. production with a wildcard origin,
    // SameSite=None without Secure, or a non-numeric TTL.
    static AuthConfig fromLookup(const Lookup& getenvLike);
    static AuthConfig fromEnvironment();
};

}  // namespace chronoplan::auth
