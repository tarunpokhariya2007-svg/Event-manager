#pragma once
#include <string>
#include <string_view>

namespace chronoplan::auth {

// RSASSA-PKCS1-v1_5 with SHA-256 (JWT "RS256") signature check with OpenSSL.
// `modulus` / `exponent` are the raw big-endian bytes of the public key (decoded JWK n / e).
// Keys shorter than 2048 bits are rejected. Never throws; any problem yields false.
bool rsaSha256Verify(const std::string& modulus, const std::string& exponent,
                     std::string_view message, std::string_view signature) noexcept;

}  // namespace chronoplan::auth
