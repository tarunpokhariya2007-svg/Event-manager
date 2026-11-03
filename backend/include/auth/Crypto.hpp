#pragma once
// Small, dependency-light crypto helpers (libsodium underneath). All token comparisons
// that involve secrets use constantTimeEquals.
#include <optional>
#include <string>
#include <string_view>

namespace chronoplan::auth {

// Must be called once before any other function here (idempotent, thread-safe).
void initCrypto();

// n random bytes from the OS CSPRNG, URL-safe base64 without padding (n = 32 -> 43 chars).
std::string randomToken(std::size_t nBytes = 32);

// Lowercase hex SHA-256 of `data` (64 chars). Used to store session/CSRF tokens at rest.
std::string sha256Hex(std::string_view data);

// Comparison whose running time does not depend on where the inputs differ.
bool constantTimeEquals(std::string_view a, std::string_view b);

std::string base64UrlEncode(std::string_view bytes);
// Accepts unpadded (or padded) URL-safe base64; nullopt on any invalid character/length.
std::optional<std::string> base64UrlDecode(std::string_view text);

}  // namespace chronoplan::auth
