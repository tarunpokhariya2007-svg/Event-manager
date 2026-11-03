// AuthService against a real temporary SQLite database (no HTTP).
#include <gtest/gtest.h>
#include "AuthTestUtil.hpp"
#include "auth/AuthErrors.hpp"
#include "auth/PasswordHasher.hpp"
#include "database/SQLiteStatement.hpp"
#include "models/Validation.hpp"

using namespace chronoplan;
using namespace chronoplan::auth;

namespace {

struct Fixture : ::testing::Test {
    testutil::TempDb tmp;
    database::DatabaseManager mgr{tmp.config()};
    AuthConfig config = authtest::testConfig();
    long long now = 1'800'000'000LL;
    std::shared_ptr<authtest::FakeGoogleVerifier> google = std::make_shared<authtest::FakeGoogleVerifier>();
    AuthService service{mgr.database(), config, google, [this] { return now; }};

    std::string scalar(const std::string& sql) {
        database::SQLiteStatement s = mgr.database().prepare(sql);
        s.step();
        return s.columnText(0);
    }
};

}  // namespace

TEST_F(Fixture, RegisterStoresAnArgon2idHashNeverThePasswordOrTheSessionToken) {
    const SignInResult r = service.registerUser("Alice@Example.com", "correct horse battery", "Alice", "1.1.1.1");
    EXPECT_TRUE(r.created);
    EXPECT_EQ(r.user.email, "alice@example.com");
    EXPECT_EQ(scalar("SELECT password_hash FROM users").rfind("$argon2id$", 0), 0u);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users WHERE password_hash LIKE '%correct horse%'"), "0");
    EXPECT_EQ(scalar("SELECT token_hash FROM sessions"), sha256Hex(r.sessionToken));
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM sessions WHERE token_hash = '" + r.sessionToken + "'"), "0");
    EXPECT_EQ(r.sessionToken.size(), 43u);
}

TEST_F(Fixture, DuplicateEmailIsRejectedCaseInsensitively) {
    service.registerUser("a@example.com", "correct horse battery", "", "ip");
    EXPECT_THROW(service.registerUser("A@EXAMPLE.COM", "another good password", "", "ip"), EmailTakenError);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users"), "1");
}

TEST_F(Fixture, RegisterValidatesInput) {
    EXPECT_THROW(service.registerUser("nope", "correct horse battery", "", "ip"), models::ValidationError);
    EXPECT_THROW(service.registerUser("a@example.com", "short", "", "ip"), models::ValidationError);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users"), "0");
}

TEST_F(Fixture, LoginSucceedsWithCorrectPasswordAndFailsGenericallyOtherwise) {
    service.registerUser("a@example.com", "correct horse battery", "", "ip");
    const SignInResult ok = service.login("A@example.com", "correct horse battery", "ip");
    EXPECT_FALSE(ok.created);
    EXPECT_TRUE(service.authenticate(ok.sessionToken).has_value());

    for (const auto& [mail, pw] : {std::pair<std::string, std::string>{"a@example.com", "wrong password!"},
                                   {"nobody@example.com", "correct horse battery"}}) {
        try {
            service.login(mail, pw, "ip2");
            FAIL() << "expected InvalidCredentialsError";
        } catch (const InvalidCredentialsError& e) {
            EXPECT_STREQ(e.what(), "incorrect e-mail or password");   // same text for unknown e-mail and bad password
        }
    }
}

TEST_F(Fixture, RepeatedFailuresAreThrottled) {
    service.registerUser("a@example.com", "correct horse battery", "", "ip");
    for (int i = 0; i < config.maxFailedLogins; ++i)
        EXPECT_THROW(service.login("a@example.com", "bad password 1", "ip"), InvalidCredentialsError);
    EXPECT_THROW(service.login("a@example.com", "correct horse battery", "ip"), TooManyAttemptsError);   // even the right password
    now += config.failedLoginWindowSeconds + 1;
    EXPECT_NO_THROW(service.login("a@example.com", "correct horse battery", "ip"));
}

TEST_F(Fixture, SessionExpiresAbsolutelyAndWhenIdle) {
    const SignInResult r = service.registerUser("a@example.com", "correct horse battery", "", "ip");
    EXPECT_TRUE(service.authenticate(r.sessionToken));
    now += config.sessionIdleSeconds - 10;
    EXPECT_TRUE(service.authenticate(r.sessionToken));          // use refreshes last_seen
    now += config.sessionIdleSeconds - 10;
    EXPECT_TRUE(service.authenticate(r.sessionToken));
    now += config.sessionIdleSeconds + 1;                       // idle too long
    EXPECT_FALSE(service.authenticate(r.sessionToken));
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM sessions"), "0");    // and it was deleted

    const SignInResult r2 = service.login("a@example.com", "correct horse battery", "ip");
    for (int i = 0; i < 20; ++i) { now += config.sessionIdleSeconds / 2; service.authenticate(r2.sessionToken); }
    EXPECT_FALSE(service.authenticate(r2.sessionToken));        // absolute lifetime reached despite constant use
}

TEST_F(Fixture, LogoutInvalidatesOnlyThatSession) {
    const SignInResult a = service.registerUser("a@example.com", "correct horse battery", "", "ip");
    const SignInResult b = service.login("a@example.com", "correct horse battery", "ip");
    service.logout(a.sessionToken);
    EXPECT_FALSE(service.authenticate(a.sessionToken));
    EXPECT_TRUE(service.authenticate(b.sessionToken));
    EXPECT_FALSE(service.authenticate("garbage"));
    EXPECT_FALSE(service.authenticate(""));
}

TEST_F(Fixture, CsrfTokenIsBoundToTheSession) {
    const SignInResult a = service.registerUser("a@example.com", "correct horse battery", "", "ip");
    const SignInResult b = service.registerUser("b@example.com", "correct horse battery", "", "ip");
    const auto ua = service.authenticate(a.sessionToken);
    ASSERT_TRUE(ua);
    EXPECT_EQ(ua->csrfToken, a.user.csrfToken);
    EXPECT_TRUE(AuthService::csrfMatches(*ua, a.user.csrfToken));
    EXPECT_FALSE(AuthService::csrfMatches(*ua, b.user.csrfToken));
    EXPECT_FALSE(AuthService::csrfMatches(*ua, ""));
    EXPECT_FALSE(AuthService::csrfMatches(*ua, a.sessionToken));   // the cookie token itself is not the CSRF token
}

TEST_F(Fixture, GoogleSignInCreatesThenReusesTheAccount) {
    google->claims = {"google-sub-1", "g@example.com", "Gina", true};
    const SignInResult first = service.loginWithGoogle("token", "ip");
    EXPECT_TRUE(first.created);
    EXPECT_TRUE(first.user.hasGoogle);
    EXPECT_FALSE(first.user.hasPassword);
    const SignInResult second = service.loginWithGoogle("token", "ip");
    EXPECT_FALSE(second.created);
    EXPECT_EQ(second.user.id, first.user.id);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users"), "1");
    // A Google-only account has no password: password sign-in fails the same generic way.
    EXPECT_THROW(service.login("g@example.com", "anything at all", "ip"), InvalidCredentialsError);
}

TEST_F(Fixture, GoogleRejectionsAndUnverifiedEmailsDoNotSignIn) {
    google->reject = true;
    EXPECT_THROW(service.loginWithGoogle("bad", "ip"), InvalidGoogleTokenError);
    google->reject = false;
    google->claims = {"sub", "g@example.com", "G", /*emailVerified=*/false};
    EXPECT_THROW(service.loginWithGoogle("t", "ip"), InvalidGoogleTokenError);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users"), "0");
}

TEST_F(Fixture, GoogleDoesNotTakeOverAnExistingPasswordAccount) {
    service.registerUser("victim@example.com", "attacker chosen pw", "", "ip");   // e-mail never verified
    google->claims = {"real-owner-sub", "victim@example.com", "Real Owner", true};
    EXPECT_THROW(service.loginWithGoogle("t", "ip"), AccountExistsError);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users"), "1");
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users WHERE google_sub IS NOT NULL"), "0");
}

TEST_F(Fixture, GoogleSignInIsUnavailableWithoutAClientId) {
    AuthConfig noGoogle = authtest::testConfig();
    noGoogle.googleClientId.clear();
    AuthService svc{mgr.database(), noGoogle, nullptr, [this] { return now; }};
    EXPECT_THROW(svc.loginWithGoogle("t", "ip"), GoogleNotConfiguredError);
}

TEST_F(Fixture, TheLockedLegacyOwnerCanNeverSignIn) {
    mgr.database().exec(
        "INSERT INTO users (id, email, display_name, is_system, created_at) VALUES (99, 'legacy@chronoplan.invalid', 'Legacy', 1, 0)");
    EXPECT_THROW(service.login("legacy@chronoplan.invalid", "whatever it is", "ip"), InvalidCredentialsError);
    mgr.database().exec("INSERT INTO sessions (user_id, token_hash, created_at, last_seen_at, expires_at) "
                        "VALUES (99, '" + sha256Hex("forged") + "', 0, " + std::to_string(now) + ", " + std::to_string(now + 1000) + ")");
    EXPECT_FALSE(service.authenticate("forged"));
}
