#pragma once
#include <string>
#include <string_view>

namespace chronoplan::auth {

// Argon2id via libsodium's crypto_pwhash_str (random salt, parameters embedded in the encoded
// string, so they can be raised later and old hashes still verify).
class PasswordHasher {
public:
    // Interactive limits (t=2, m=64 MiB): above the OWASP Argon2id minimum, ~50-100 ms per hash.
    static std::string hash(std::string_view password);
    static bool verify(const std::string& encodedHash, std::string_view password);
    // True when the stored hash was made with weaker parameters than hash() uses today.
    static bool needsRehash(const std::string& encodedHash);
    // A valid hash of a random password; verified against when the e-mail is unknown so that
    // "no such user" and "wrong password" take the same time.
    static const std::string& dummyHash();
};

}  // namespace chronoplan::auth
