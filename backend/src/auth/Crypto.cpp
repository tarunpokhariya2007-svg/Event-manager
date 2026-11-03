#include "auth/Crypto.hpp"
#include <mutex>
#include <stdexcept>
#include <vector>
#include <sodium.h>

namespace chronoplan::auth {

void initCrypto() {
    static std::once_flag once;
    std::call_once(once, [] {
        if (sodium_init() < 0) throw std::runtime_error("libsodium failed to initialise");
    });
}

namespace {
constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
}

std::string base64UrlEncode(std::string_view bytes) {
    std::string out;
    out.reserve((bytes.size() * 4 + 2) / 3);
    std::size_t i = 0;
    for (; i + 2 < bytes.size(); i += 3) {
        const unsigned v = (static_cast<unsigned char>(bytes[i]) << 16) |
                           (static_cast<unsigned char>(bytes[i + 1]) << 8) |
                           static_cast<unsigned char>(bytes[i + 2]);
        out += kAlphabet[(v >> 18) & 63]; out += kAlphabet[(v >> 12) & 63];
        out += kAlphabet[(v >> 6) & 63];  out += kAlphabet[v & 63];
    }
    if (i + 1 == bytes.size()) {
        const unsigned v = static_cast<unsigned char>(bytes[i]) << 16;
        out += kAlphabet[(v >> 18) & 63]; out += kAlphabet[(v >> 12) & 63];
    } else if (i + 2 == bytes.size()) {
        const unsigned v = (static_cast<unsigned char>(bytes[i]) << 16) |
                           (static_cast<unsigned char>(bytes[i + 1]) << 8);
        out += kAlphabet[(v >> 18) & 63]; out += kAlphabet[(v >> 12) & 63]; out += kAlphabet[(v >> 6) & 63];
    }
    return out;
}

std::optional<std::string> base64UrlDecode(std::string_view text) {
    while (!text.empty() && text.back() == '=') text.remove_suffix(1);
    if (text.size() % 4 == 1) return std::nullopt;
    std::string out;
    out.reserve(text.size() * 3 / 4);
    unsigned acc = 0;
    int bits = 0;
    for (const char c : text) {
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '-') v = 62;
        else if (c == '_') v = 63;
        else return std::nullopt;
        acc = (acc << 6) | static_cast<unsigned>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((acc >> bits) & 0xFF);
        }
    }
    return out;
}

std::string randomToken(std::size_t nBytes) {
    initCrypto();
    std::string raw(nBytes, '\0');
    randombytes_buf(raw.data(), raw.size());
    return base64UrlEncode(raw);
}

std::string sha256Hex(std::string_view data) {
    initCrypto();
    unsigned char digest[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(digest, reinterpret_cast<const unsigned char*>(data.data()), data.size());
    char hex[crypto_hash_sha256_BYTES * 2 + 1];
    sodium_bin2hex(hex, sizeof hex, digest, sizeof digest);
    return std::string(hex);
}

bool constantTimeEquals(std::string_view a, std::string_view b) {
    initCrypto();
    if (a.size() != b.size()) return false;   // lengths are not secret (hashes have fixed size)
    if (a.empty()) return true;
    return sodium_memcmp(a.data(), b.data(), a.size()) == 0;
}

}  // namespace chronoplan::auth
