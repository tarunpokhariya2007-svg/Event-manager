#include "auth/AuthConfig.hpp"
#include <cstdlib>
#include <stdexcept>

namespace chronoplan::auth {

namespace {
bool parseBool(const char* name, const char* text) {
    const std::string s = text;
    if (s == "1" || s == "true" || s == "TRUE") return true;
    if (s == "0" || s == "false" || s == "FALSE") return false;
    throw std::invalid_argument(std::string(name) + " must be true or false");
}
long long parseSeconds(const char* name, const char* text, long long lo, long long hi) {
    const std::string s = text;
    if (s.empty() || s.size() > 10 || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument(std::string(name) + " must be an integer number of seconds");
    const long long v = std::stoll(s);
    if (v < lo || v > hi) throw std::invalid_argument(std::string(name) + " is out of range");
    return v;
}
}  // namespace

AuthConfig AuthConfig::fromLookup(const Lookup& get) {
    AuthConfig c;
    if (const char* e = get("CHRONOPLAN_ENV"); e && *e) {
        const std::string env = e;
        if (env == "production") c.production = true;
        else if (env != "development") throw std::invalid_argument("CHRONOPLAN_ENV must be 'development' or 'production'");
    }
    if (const char* o = get("CHRONOPLAN_CORS_ORIGIN"); o && *o) c.frontendOrigin = o;
    if (const char* g = get("GOOGLE_CLIENT_ID"); g && *g) c.googleClientId = g;
    c.cookieSecure = c.production;
    if (const char* s = get("CHRONOPLAN_COOKIE_SECURE"); s && *s) c.cookieSecure = parseBool("CHRONOPLAN_COOKIE_SECURE", s);
    if (const char* s = get("CHRONOPLAN_COOKIE_SAMESITE"); s && *s) {
        const std::string v = s;
        if (v != "Lax" && v != "Strict" && v != "None")
            throw std::invalid_argument("CHRONOPLAN_COOKIE_SAMESITE must be Lax, Strict or None");
        c.cookieSameSite = v;
    }
    if (const char* s = get("CHRONOPLAN_TRUST_X_FORWARDED_FOR"); s && *s) c.trustForwardedFor = parseBool("CHRONOPLAN_TRUST_X_FORWARDED_FOR", s);
    if (const char* s = get("CHRONOPLAN_SESSION_TTL_SECONDS"); s && *s)
        c.sessionTtlSeconds = parseSeconds("CHRONOPLAN_SESSION_TTL_SECONDS", s, 300, 365LL * 24 * 3600);
    if (const char* s = get("CHRONOPLAN_SESSION_IDLE_SECONDS"); s && *s)
        c.sessionIdleSeconds = parseSeconds("CHRONOPLAN_SESSION_IDLE_SECONDS", s, 60, 365LL * 24 * 3600);

    if (c.frontendOrigin.empty() && !c.production) c.frontendOrigin = "http://localhost:5173";   // Vite dev server
    if (c.frontendOrigin.empty() || c.frontendOrigin == "*")
        throw std::invalid_argument(
            "CHRONOPLAN_CORS_ORIGIN must be the exact frontend origin (e.g. https://app.example.com); "
            "'*' cannot be combined with cookie authentication");
    if (c.cookieSameSite == "None" && !c.cookieSecure)
        throw std::invalid_argument("CHRONOPLAN_COOKIE_SAMESITE=None requires CHRONOPLAN_COOKIE_SECURE=true");
    if (c.production && !c.cookieSecure)
        throw std::invalid_argument("CHRONOPLAN_COOKIE_SECURE must be true when CHRONOPLAN_ENV=production");
    if (c.production && c.frontendOrigin.rfind("https://", 0) != 0)
        throw std::invalid_argument("CHRONOPLAN_CORS_ORIGIN must be an https:// origin when CHRONOPLAN_ENV=production");
    return c;
}

AuthConfig AuthConfig::fromEnvironment() {
    return fromLookup([](const char* name) { return std::getenv(name); });
}

}  // namespace chronoplan::auth
