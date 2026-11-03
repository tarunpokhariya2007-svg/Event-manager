#include "auth/GoogleTokenVerifier.hpp"
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include "auth/AuthErrors.hpp"
#include "auth/Crypto.hpp"
#include "auth/RsaVerify.hpp"

namespace chronoplan::auth {

using nlohmann::json;

namespace {
constexpr std::size_t kMaxTokenBytes = 8192;
constexpr long long kClockSkewSeconds = 60;

[[noreturn]] void reject(const char* why) { throw InvalidGoogleTokenError(why); }

json parseSegment(const std::string& b64, const char* what) {
    const auto raw = base64UrlDecode(b64);
    if (!raw) reject(what);
    json j = json::parse(*raw, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) reject(what);
    return j;
}

std::string stringClaim(const json& j, const char* key) {
    const auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}
}  // namespace

GoogleTokenVerifier::GoogleTokenVerifier(std::string clientId, std::shared_ptr<IJwksProvider> keys, Clock clock)
    : clientId_(std::move(clientId)), keys_(std::move(keys)), clock_(std::move(clock)) {}

GoogleClaims GoogleTokenVerifier::verify(const std::string& token) {
    if (token.empty() || token.size() > kMaxTokenBytes) reject("malformed token");
    const auto d1 = token.find('.');
    const auto d2 = d1 == std::string::npos ? std::string::npos : token.find('.', d1 + 1);
    if (d1 == std::string::npos || d2 == std::string::npos || token.find('.', d2 + 1) != std::string::npos ||
        d1 == 0 || d2 == d1 + 1 || d2 + 1 == token.size())
        reject("malformed token");
    const std::string headerB64 = token.substr(0, d1);
    const std::string payloadB64 = token.substr(d1 + 1, d2 - d1 - 1);
    const std::string sigB64 = token.substr(d2 + 1);

    // 1. Header: only RS256 is accepted ("none"/HS256 downgrade attacks stop here).
    const json header = parseSegment(headerB64, "malformed token header");
    if (stringClaim(header, "alg") != "RS256") reject("unsupported token algorithm");
    const std::string kid = stringClaim(header, "kid");
    if (kid.empty() || kid.size() > 128) reject("token has no key id");

    // 2. Signature, against Google's published key for that kid.
    const auto key = keys_->findKey(kid);
    if (!key) reject("unknown signing key");
    const auto sig = base64UrlDecode(sigB64);
    if (!sig) reject("malformed token signature");
    const std::string signedPart = headerB64 + "." + payloadB64;
    if (!rsaSha256Verify(key->modulus, key->exponent, signedPart, *sig)) reject("invalid token signature");

    // 3. Claims (only trusted now that the signature is valid).
    const json p = parseSegment(payloadB64, "malformed token payload");
    const std::string iss = stringClaim(p, "iss");
    if (iss != "https://accounts.google.com" && iss != "accounts.google.com") reject("wrong token issuer");
    if (stringClaim(p, "aud") != clientId_ || clientId_.empty()) reject("token was issued for a different client");

    const long long now = clock_();
    const auto exp = p.find("exp");
    if (exp == p.end() || !exp->is_number_integer()) reject("token has no expiry");
    if (now > exp->get<long long>() + kClockSkewSeconds) reject("token has expired");
    if (const auto iat = p.find("iat"); iat != p.end()) {
        if (!iat->is_number_integer() || iat->get<long long>() > now + kClockSkewSeconds * 5) reject("token issued in the future");
    }

    GoogleClaims c;
    c.sub = stringClaim(p, "sub");
    if (c.sub.empty() || c.sub.size() > 255) reject("token has no subject");
    c.email = stringClaim(p, "email");
    if (c.email.empty() || c.email.size() > 254) reject("token has no e-mail address");
    std::transform(c.email.begin(), c.email.end(), c.email.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    c.name = stringClaim(p, "name");
    const auto ev = p.find("email_verified");   // Google sends a boolean, older tokens the string "true"
    c.emailVerified = ev != p.end() && ((ev->is_boolean() && ev->get<bool>()) || (ev->is_string() && ev->get<std::string>() == "true"));
    return c;
}

}  // namespace chronoplan::auth
