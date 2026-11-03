#pragma once
#include <string>

namespace chronoplan::auth {

constexpr std::size_t kMinPasswordLength = 8;
constexpr std::size_t kMaxPasswordBytes = 128;   // bounds Argon2 work per request

// Trims and lowercases. Returns the canonical e-mail or throws models::ValidationError.
std::string normalizeAndValidateEmail(const std::string& raw);
// Throws models::ValidationError with a message that is safe to show to the user.
void validatePassword(const std::string& password, const std::string& normalizedEmail);
// Display name: trimmed, control characters removed, at most 100 bytes.
std::string sanitizeDisplayName(const std::string& raw);

}  // namespace chronoplan::auth
