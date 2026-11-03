#include "auth/Cookies.hpp"

namespace chronoplan::auth {

std::optional<std::string> findCookie(std::string_view header, std::string_view name) {
    std::size_t pos = 0;
    while (pos < header.size()) {
        while (pos < header.size() && (header[pos] == ' ' || header[pos] == ';')) ++pos;
        const std::size_t end = header.find(';', pos);
        const std::string_view pair = header.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
        const std::size_t eq = pair.find('=');
        if (eq != std::string_view::npos && pair.substr(0, eq) == name) {
            std::string_view v = pair.substr(eq + 1);
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
            return std::string(v);
        }
        if (end == std::string_view::npos) break;
        pos = end + 1;
    }
    return std::nullopt;
}

namespace {
std::string attributes(const AuthConfig& c, long long maxAge) {
    std::string a = "; Path=/; Max-Age=" + std::to_string(maxAge) + "; HttpOnly; SameSite=" + c.cookieSameSite;
    if (c.cookieSecure) a += "; Secure";
    return a;
}
}  // namespace

std::string buildSessionCookie(const AuthConfig& c, const std::string& token, long long maxAgeSeconds) {
    return c.cookieName + "=" + token + attributes(c, maxAgeSeconds);
}

std::string buildClearedSessionCookie(const AuthConfig& c) {
    return c.cookieName + "=" + attributes(c, 0);
}

}  // namespace chronoplan::auth
