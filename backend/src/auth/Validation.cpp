#include "auth/Validation.hpp"
#include <algorithm>
#include <cctype>
#include "models/Validation.hpp"

namespace chronoplan::auth {

using models::ValidationError;

std::string normalizeAndValidateEmail(const std::string& raw) {
    std::size_t b = 0, e = raw.size();
    while (b < e && std::isspace(static_cast<unsigned char>(raw[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(raw[e - 1]))) --e;
    std::string s = raw.substr(b, e - b);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Pragmatic check, not a full RFC 5322 parser: one '@', sane lengths, ASCII atom characters,
    // a dotted domain. Real ownership is only proven by e-mail verification / Google.
    const std::string msg = "enter a valid e-mail address";
    if (s.size() < 3 || s.size() > 254) throw ValidationError(msg);
    const auto at = s.find('@');
    if (at == std::string::npos || at != s.rfind('@') || at == 0 || at > 64) throw ValidationError(msg);
    const std::string local = s.substr(0, at), domain = s.substr(at + 1);
    if (domain.size() < 3 || domain.find('.') == std::string::npos) throw ValidationError(msg);
    for (const unsigned char c : local)
        if (!(std::isalnum(c) || std::string(".!#$%&'*+/=?^_`{|}~-").find(static_cast<char>(c)) != std::string::npos))
            throw ValidationError(msg);
    if (local.front() == '.' || local.back() == '.' || local.find("..") != std::string::npos) throw ValidationError(msg);
    if (domain.front() == '.' || domain.back() == '.' || domain.front() == '-' || domain.find("..") != std::string::npos)
        throw ValidationError(msg);
    for (const unsigned char c : domain)
        if (!(std::isalnum(c) || c == '.' || c == '-')) throw ValidationError(msg);
    return s;
}

void validatePassword(const std::string& password, const std::string& normalizedEmail) {
    if (password.size() < kMinPasswordLength)
        throw ValidationError("password must be at least " + std::to_string(kMinPasswordLength) + " characters");
    if (password.size() > kMaxPasswordBytes)
        throw ValidationError("password must be at most " + std::to_string(kMaxPasswordBytes) + " bytes");
    if (password.find('\0') != std::string::npos) throw ValidationError("password contains invalid characters");
    std::string lower = password;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == normalizedEmail) throw ValidationError("password must not be your e-mail address");
    if (std::all_of(password.begin(), password.end(), [&](char c) { return c == password[0]; }))
        throw ValidationError("password is too simple");
}

std::string sanitizeDisplayName(const std::string& raw) {
    std::string out;
    for (const unsigned char c : raw)
        if (c >= 0x20 && c != 0x7F) out += static_cast<char>(c);
    std::size_t b = 0, e = out.size();
    while (b < e && out[b] == ' ') ++b;
    while (e > b && out[e - 1] == ' ') --e;
    out = out.substr(b, e - b);
    if (out.size() > 100) {
        out.resize(100);
        while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80) out.pop_back();   // don't cut a UTF-8 sequence
        if (!out.empty() && static_cast<unsigned char>(out.back()) >= 0xC0) out.pop_back();
    }
    return out;
}

}  // namespace chronoplan::auth
