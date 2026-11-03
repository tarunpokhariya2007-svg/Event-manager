#pragma once
// Verifies a Google Identity Services ID token (a JWT) on the server. Nothing the browser says
// about the user is trusted: identity comes only from the verified token claims.
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace chronoplan::auth {

struct GoogleClaims {
    std::string sub;          // stable Google account id
    std::string email;        // lower-case
    std::string name;
    bool emailVerified = false;
};

struct RsaPublicKey {
    std::string modulus;      // raw big-endian bytes (decoded JWK "n")
    std::string exponent;     // raw big-endian bytes (decoded JWK "e")
};

// Source of Google's signing keys, looked up by the token header's "kid".
class IJwksProvider {
public:
    virtual ~IJwksProvider() = default;
    // nullopt when the kid is unknown. Throws GoogleUnavailableError when keys cannot be obtained.
    virtual std::optional<RsaPublicKey> findKey(const std::string& kid) = 0;
};

class IGoogleTokenVerifier {
public:
    virtual ~IGoogleTokenVerifier() = default;
    // Throws InvalidGoogleTokenError (bad signature/issuer/audience/expiry/claims) or
    // GoogleUnavailableError. Returns only claims from a fully verified token.
    virtual GoogleClaims verify(const std::string& idToken) = 0;
};

class GoogleTokenVerifier : public IGoogleTokenVerifier {
public:
    using Clock = std::function<long long()>;   // unix seconds
    GoogleTokenVerifier(std::string clientId, std::shared_ptr<IJwksProvider> keys, Clock clock);
    GoogleClaims verify(const std::string& idToken) override;

private:
    std::string clientId_;
    std::shared_ptr<IJwksProvider> keys_;
    Clock clock_;
};

// Fetches https://www.googleapis.com/oauth2/v3/certs with libcurl (TLS verification on, HTTPS only,
// size/time limits), caches the keys for the response's Cache-Control max-age, and refetches at
// most once a minute when an unknown kid shows up (key rotation).
class HttpJwksProvider : public IJwksProvider {
public:
    using Clock = GoogleTokenVerifier::Clock;
    explicit HttpJwksProvider(Clock clock, std::string url = "https://www.googleapis.com/oauth2/v3/certs");
    std::optional<RsaPublicKey> findKey(const std::string& kid) override;

private:
    struct Entry { std::string kid; RsaPublicKey key; };
    std::optional<RsaPublicKey> lookup(const std::string& kid) const;   // caller holds mutex_
    void refresh();                                                      // caller holds mutex_

    Clock clock_;
    std::string url_;
    std::mutex mutex_;
    std::vector<Entry> keys_;
    long long fetchedAt_ = 0;    // 0 = never fetched
    long long validUntil_ = 0;
};

}  // namespace chronoplan::auth
