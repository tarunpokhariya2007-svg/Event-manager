#pragma once
#include <optional>
#include <string>
#include <string_view>
#include "auth/AuthConfig.hpp"

namespace chronoplan::auth {

// Value of cookie `name` from a request "Cookie:" header; nullopt when absent.
std::optional<std::string> findCookie(std::string_view cookieHeader, std::string_view name);

// "name=token; Path=/; Max-Age=N; HttpOnly; SameSite=...[; Secure]". The token is URL-safe base64,
// so it needs no escaping.
std::string buildSessionCookie(const AuthConfig& config, const std::string& token, long long maxAgeSeconds);
// Same attributes with an empty value and Max-Age=0 (the browser deletes the cookie).
std::string buildClearedSessionCookie(const AuthConfig& config);

}  // namespace chronoplan::auth
