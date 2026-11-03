// GoogleTokenVerifier with a locally generated RSA key standing in for Google's signing key.
#include <gtest/gtest.h>
#include "AuthTestUtil.hpp"

using namespace chronoplan::auth;
using authtest::kClientId;
using nlohmann::json;

namespace {

struct Fixture : ::testing::Test {
    authtest::TestRsaKey key;
    authtest::TestRsaKey otherKey;
    std::shared_ptr<authtest::FakeJwks> jwks = std::make_shared<authtest::FakeJwks>();
    long long now = 1'800'000'000LL;
    GoogleTokenVerifier verifier{kClientId, jwks, [this] { return now; }};

    Fixture() { jwks->key = {key.modulus, key.exponent}; }

    json header() const { return {{"alg", "RS256"}, {"kid", "test-kid"}, {"typ", "JWT"}}; }
    json payload() const {
        return {{"iss", "https://accounts.google.com"}, {"aud", kClientId}, {"sub", "1234567890"},
                {"email", "Person@Example.com"}, {"email_verified", true}, {"name", "Pat Person"},
                {"iat", now - 10}, {"exp", now + 3600}};
    }
    std::string token(json h, json p) const { return key.makeJwt(h, p); }
};

}  // namespace

TEST_F(Fixture, AcceptsAValidToken) {
    const GoogleClaims c = verifier.verify(token(header(), payload()));
    EXPECT_EQ(c.sub, "1234567890");
    EXPECT_EQ(c.email, "person@example.com");
    EXPECT_EQ(c.name, "Pat Person");
    EXPECT_TRUE(c.emailVerified);
}

TEST_F(Fixture, AcceptsTheOtherGoogleIssuerSpelling) {
    json p = payload(); p["iss"] = "accounts.google.com";
    EXPECT_NO_THROW(verifier.verify(token(header(), p)));
}

TEST_F(Fixture, RejectsABadSignature) {
    json p = payload();
    const std::string good = token(header(), p);
    // Signed by a different key.
    EXPECT_THROW(verifier.verify(otherKey.makeJwt(header(), p)), InvalidGoogleTokenError);
    // Payload altered after signing (e.g. swapped e-mail).
    const auto d1 = good.find('.'), d2 = good.rfind('.');
    p["email"] = "admin@example.com";
    const std::string forged = good.substr(0, d1 + 1) + base64UrlEncode(p.dump()) + good.substr(d2);
    EXPECT_THROW(verifier.verify(forged), InvalidGoogleTokenError);
    // Signature bytes damaged.
    std::string damaged = good;
    damaged.back() = damaged.back() == 'A' ? 'B' : 'A';
    EXPECT_THROW(verifier.verify(damaged), InvalidGoogleTokenError);
}

TEST_F(Fixture, RejectsWrongIssuerAudienceAndExpiry) {
    json p = payload(); p["iss"] = "https://evil.example.com";
    EXPECT_THROW(verifier.verify(token(header(), p)), InvalidGoogleTokenError);
    p = payload(); p["aud"] = "someone-elses-client";
    EXPECT_THROW(verifier.verify(token(header(), p)), InvalidGoogleTokenError);
    p = payload(); p["aud"] = json::array({kClientId});
    EXPECT_THROW(verifier.verify(token(header(), p)), InvalidGoogleTokenError);
    p = payload(); p["exp"] = now - 61;
    EXPECT_THROW(verifier.verify(token(header(), p)), InvalidGoogleTokenError);
    p = payload(); p.erase("exp");
    EXPECT_THROW(verifier.verify(token(header(), p)), InvalidGoogleTokenError);
    p = payload(); p["iat"] = now + 100000;
    EXPECT_THROW(verifier.verify(token(header(), p)), InvalidGoogleTokenError);
}

TEST_F(Fixture, RejectsMissingClaims) {
    for (const char* claim : {"sub", "email"}) {
        json p = payload(); p.erase(claim);
        EXPECT_THROW(verifier.verify(token(header(), p)), InvalidGoogleTokenError) << claim;
    }
}

TEST_F(Fixture, ReportsUnverifiedEmailsInTheClaims) {
    json p = payload(); p["email_verified"] = false;
    EXPECT_FALSE(verifier.verify(token(header(), p)).emailVerified);
    p["email_verified"] = "true";
    EXPECT_TRUE(verifier.verify(token(header(), p)).emailVerified);
}

TEST_F(Fixture, RejectsOtherAlgorithmsAndUnknownKeys) {
    json h = header(); h["alg"] = "none";
    EXPECT_THROW(verifier.verify(token(h, payload())), InvalidGoogleTokenError);
    h = header(); h["alg"] = "HS256";
    EXPECT_THROW(verifier.verify(token(h, payload())), InvalidGoogleTokenError);
    h = header(); h["kid"] = "rotated-away";
    EXPECT_THROW(verifier.verify(token(h, payload())), InvalidGoogleTokenError);
    h = header(); h.erase("kid");
    EXPECT_THROW(verifier.verify(token(h, payload())), InvalidGoogleTokenError);
}

TEST_F(Fixture, RejectsMalformedTokens) {
    for (const char* bad : {"", "abc", "a.b", "a.b.c.d", "..", "a..c", "!!!.???.***"})
        EXPECT_THROW(verifier.verify(bad), InvalidGoogleTokenError) << bad;
    EXPECT_THROW(verifier.verify(std::string(9000, 'a') + ".b.c"), InvalidGoogleTokenError);
}

TEST_F(Fixture, KeyFetchFailureIsAnAvailabilityErrorNotAnInvalidToken) {
    jwks->unavailable = true;
    EXPECT_THROW(verifier.verify(token(header(), payload())), GoogleUnavailableError);
}

TEST(GoogleVerifier, RefusesToWorkWithoutAClientId) {
    authtest::TestRsaKey key;
    auto jwks = std::make_shared<authtest::FakeJwks>();
    jwks->key = {key.modulus, key.exponent};
    GoogleTokenVerifier v("", jwks, [] { return 1'800'000'000LL; });
    const json p = {{"iss", "accounts.google.com"}, {"aud", ""}, {"sub", "s"}, {"email", "a@b.com"}, {"exp", 1'800'009'999LL}};
    EXPECT_THROW(v.verify(key.makeJwt({{"alg", "RS256"}, {"kid", "test-kid"}}, p)), InvalidGoogleTokenError);
}
