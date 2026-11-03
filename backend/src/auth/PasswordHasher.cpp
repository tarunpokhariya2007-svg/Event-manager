#include "auth/PasswordHasher.hpp"
#include <stdexcept>
#include <sodium.h>
#include "auth/Crypto.hpp"

namespace chronoplan::auth {

std::string PasswordHasher::hash(std::string_view password) {
    initCrypto();
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(out, password.data(), password.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
        throw std::runtime_error("password hashing failed (out of memory)");
    return std::string(out);
}

bool PasswordHasher::verify(const std::string& encodedHash, std::string_view password) {
    initCrypto();
    return crypto_pwhash_str_verify(encodedHash.c_str(), password.data(), password.size()) == 0;
}

bool PasswordHasher::needsRehash(const std::string& encodedHash) {
    initCrypto();
    return crypto_pwhash_str_needs_rehash(encodedHash.c_str(), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0;
}

const std::string& PasswordHasher::dummyHash() {
    static const std::string h = hash(randomToken(24));
    return h;
}

}  // namespace chronoplan::auth
