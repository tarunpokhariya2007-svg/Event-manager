// Pure authentication building blocks: crypto helpers, Argon2id, validation, cookies,
// throttling and configuration. No database, no HTTP.
#include <set>
#include <gtest/gtest.h>
#include "auth/AuthConfig.hpp"
#include "auth/Cookies.hpp"
#include "auth/Crypto.hpp"
#include "auth/PasswordHasher.hpp"
#include "auth/RateLimiter.hpp"
#include "auth/Validation.hpp"
#include "models/Validation.hpp"

using namespace chronoplan::auth;
using chronoplan::models::ValidationError;

// ---- Crypto ----
TEST(Crypto, Sha256MatchesKnownVectors) {
    EXPECT_EQ(sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Crypto, RandomTokensAreLongUrlSafeAndUnique) {
    std::set<std::string> seen;
    for (int i = 0; i < 200; ++i) {
        const std::string t = randomToken(32);
        EXPECT_EQ(t.size(), 43u);
        EXPECT_EQ(t.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"), std::string::npos);
        EXPECT_TRUE(seen.insert(t).second);
    }
}

TEST(Crypto, Base64UrlRoundTripsAndRejectsGarbage) {
    for (const std::string s : {std::string(), std::string("f"), std::string("fo"), std::string("foo"), std::string("foob"),
                                std::string("\xff\xfe\x00\x01", 4)})
        EXPECT_EQ(base64UrlDecode(base64UrlEncode(s)), s) << s.size();
    EXPECT_EQ(base64UrlEncode("foo"), "Zm9v");
    EXPECT_EQ(base64UrlEncode("fo"), "Zm8");   // no padding
    EXPECT_FALSE(base64UrlDecode("a+b/").has_value());   // standard alphabet is not URL-safe
    EXPECT_FALSE(base64UrlDecode("abcde").has_value());  // impossible length
    EXPECT_FALSE(base64UrlDecode("ab cd").has_value());
}

TEST(Crypto, ConstantTimeEquals) {
    EXPECT_TRUE(constantTimeEquals("abc", "abc"));
    EXPECT_FALSE(constantTimeEquals("abc", "abd"));
    EXPECT_FALSE(constantTimeEquals("abc", "abcd"));
    EXPECT_TRUE(constantTimeEquals("", ""));
}

// ---- Argon2id ----
TEST(PasswordHasher, HashesWithArgon2idAndVerifies) {
    const std::string h = PasswordHasher::hash("correct horse battery");
    EXPECT_EQ(h.rfind("$argon2id$", 0), 0u);
    EXPECT_EQ(h.find("correct horse"), std::string::npos);   // never plaintext
    EXPECT_TRUE(PasswordHasher::verify(h, "correct horse battery"));
    EXPECT_FALSE(PasswordHasher::verify(h, "correct horse batterY"));
    EXPECT_FALSE(PasswordHasher::verify("not a hash", "x"));
    EXPECT_FALSE(PasswordHasher::needsRehash(h));
}

TEST(PasswordHasher, SameInputGivesDifferentSalts) {
    EXPECT_NE(PasswordHasher::hash("same password 1"), PasswordHasher::hash("same password 1"));
}

// ---- Validation ----
TEST(Validation, EmailIsTrimmedLowercasedAndChecked) {
    EXPECT_EQ(normalizeAndValidateEmail("  Alice@Example.COM "), "alice@example.com");
    EXPECT_EQ(normalizeAndValidateEmail("a.b+tag@sub.example.org"), "a.b+tag@sub.example.org");
    for (const char* bad : {"", "plain", "@example.com", "a@", "a@b", "a@@b.com", "a b@c.com", "a@b..com", ".a@b.com",
                            "a..b@c.com", "a@-b.com", "a@b.com.", "a@b_c.com", "<script>@x.com"})
        EXPECT_THROW(normalizeAndValidateEmail(bad), ValidationError) << bad;
    EXPECT_THROW(normalizeAndValidateEmail(std::string(250, 'a') + "@b.com"), ValidationError);
}

TEST(Validation, PasswordRules) {
    EXPECT_NO_THROW(validatePassword("long enough pw", "a@b.com"));
    EXPECT_THROW(validatePassword("short", "a@b.com"), ValidationError);
    EXPECT_THROW(validatePassword(std::string(129, 'x') + "y", "a@b.com"), ValidationError);
    EXPECT_THROW(validatePassword("aaaaaaaaaa", "a@b.com"), ValidationError);
    EXPECT_THROW(validatePassword("Alice@Example.com", "alice@example.com"), ValidationError);
    EXPECT_THROW(validatePassword(std::string("abcdefgh\0ij", 11), "a@b.com"), ValidationError);
}

TEST(Validation, DisplayNameIsCleaned) {
    EXPECT_EQ(sanitizeDisplayName("  Ada \x01Lovelace\n "), "Ada Lovelace");
    EXPECT_EQ(sanitizeDisplayName(std::string(300, 'x')).size(), 100u);
}

// ---- Cookies ----
TEST(Cookies, FindsTheNamedCookie) {
    EXPECT_EQ(findCookie("a=1; chronoplan_session=tok; b=2", "chronoplan_session"), "tok");
    EXPECT_EQ(findCookie("chronoplan_session=tok", "chronoplan_session"), "tok");
    EXPECT_EQ(findCookie("x_chronoplan_session=evil; chronoplan_session=good", "chronoplan_session"), "good");
    EXPECT_FALSE(findCookie("a=1; b=2", "chronoplan_session").has_value());
    EXPECT_FALSE(findCookie("", "chronoplan_session").has_value());
}

TEST(Cookies, SessionCookieIsHttpOnlySameSiteAndSecureWhenConfigured) {
    AuthConfig c;
    c.cookieSecure = false;
    std::string v = buildSessionCookie(c, "tok", 120);
    EXPECT_NE(v.find("chronoplan_session=tok"), std::string::npos);
    EXPECT_NE(v.find("HttpOnly"), std::string::npos);
    EXPECT_NE(v.find("SameSite=Lax"), std::string::npos);
    EXPECT_NE(v.find("Max-Age=120"), std::string::npos);
    EXPECT_NE(v.find("Path=/"), std::string::npos);
    EXPECT_EQ(v.find("Secure"), std::string::npos);
    c.cookieSecure = true;
    EXPECT_NE(buildSessionCookie(c, "tok", 120).find("; Secure"), std::string::npos);
    const std::string cleared = buildClearedSessionCookie(c);
    EXPECT_NE(cleared.find("Max-Age=0"), std::string::npos);
    EXPECT_NE(cleared.find("chronoplan_session=;"), std::string::npos);
}

// ---- RateLimiter ----
TEST(RateLimiter, BlocksAfterTheLimitAndRecoversWhenTheWindowPasses) {
    long long now = 1000;
    RateLimiter rl(3, 60, [&] { return now; });
    EXPECT_TRUE(rl.allowed("k"));
    for (int i = 0; i < 3; ++i) rl.recordFailure("k");
    EXPECT_FALSE(rl.allowed("k"));
    EXPECT_TRUE(rl.allowed("other"));
    now += 61;
    EXPECT_TRUE(rl.allowed("k"));
}

TEST(RateLimiter, ResetClearsTheCounter) {
    long long now = 0;
    RateLimiter rl(2, 60, [&] { return now; });
    rl.recordFailure("k"); rl.recordFailure("k");
    EXPECT_FALSE(rl.allowed("k"));
    rl.reset("k");
    EXPECT_TRUE(rl.allowed("k"));
}

// ---- AuthConfig ----
namespace {
AuthConfig load(std::initializer_list<std::pair<const char*, const char*>> vars) {
    return AuthConfig::fromLookup([&](const char* name) -> const char* {
        for (const auto& kv : vars) if (std::string(kv.first) == name) return kv.second;
        return nullptr;
    });
}
}  // namespace

TEST(AuthConfig, DevelopmentDefaults) {
    const AuthConfig c = load({});
    EXPECT_FALSE(c.production);
    EXPECT_EQ(c.frontendOrigin, "http://localhost:5173");
    EXPECT_FALSE(c.cookieSecure);
    EXPECT_EQ(c.cookieSameSite, "Lax");
    EXPECT_TRUE(c.googleClientId.empty());
}

TEST(AuthConfig, ProductionRequiresSecureCookiesAndAnExactHttpsOrigin) {
    const AuthConfig c = load({{"CHRONOPLAN_ENV", "production"}, {"CHRONOPLAN_CORS_ORIGIN", "https://app.example.com"}});
    EXPECT_TRUE(c.cookieSecure);
    EXPECT_THROW(load({{"CHRONOPLAN_ENV", "production"}}), std::invalid_argument);   // no origin
    EXPECT_THROW(load({{"CHRONOPLAN_ENV", "production"}, {"CHRONOPLAN_CORS_ORIGIN", "*"}}), std::invalid_argument);
    EXPECT_THROW(load({{"CHRONOPLAN_ENV", "production"}, {"CHRONOPLAN_CORS_ORIGIN", "http://app.example.com"}}), std::invalid_argument);
    EXPECT_THROW(load({{"CHRONOPLAN_ENV", "production"}, {"CHRONOPLAN_CORS_ORIGIN", "https://a.example"},
                       {"CHRONOPLAN_COOKIE_SECURE", "false"}}), std::invalid_argument);
}

TEST(AuthConfig, WildcardOriginAndBadValuesAreRejected) {
    EXPECT_THROW(load({{"CHRONOPLAN_CORS_ORIGIN", "*"}}), std::invalid_argument);
    EXPECT_THROW(load({{"CHRONOPLAN_COOKIE_SAMESITE", "Sometimes"}}), std::invalid_argument);
    EXPECT_THROW(load({{"CHRONOPLAN_COOKIE_SAMESITE", "None"}}), std::invalid_argument);   // None needs Secure
    EXPECT_NO_THROW(load({{"CHRONOPLAN_COOKIE_SAMESITE", "None"}, {"CHRONOPLAN_COOKIE_SECURE", "true"}}));
    EXPECT_THROW(load({{"CHRONOPLAN_SESSION_TTL_SECONDS", "abc"}}), std::invalid_argument);
    EXPECT_THROW(load({{"CHRONOPLAN_ENV", "staging"}}), std::invalid_argument);
}
