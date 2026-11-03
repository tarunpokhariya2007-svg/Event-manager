// The authentication HTTP API through the real Crow router (middleware, cookies, CSRF, CORS).
#include <gtest/gtest.h>
#include "AuthTestUtil.hpp"

using namespace authtest;
using nlohmann::json;
using crow::HTTPMethod;

namespace {
struct Api : ::testing::Test {
    AuthHarness h;
    static std::string creds(const std::string& e, const std::string& p) { return json{{"email", e}, {"password", p}}.dump(); }
};
}  // namespace

TEST_F(Api, RegisterCreatesAccountAndSetsAnHttpOnlyCookie) {
    const Reply r = h.post("/api/v1/auth/register", creds("Ann@Example.com", "correct horse battery"));
    ASSERT_EQ(r.code, 201) << r.raw;
    EXPECT_EQ(r.body["user"]["email"], "ann@example.com");
    EXPECT_TRUE(r.body.contains("csrf_token"));
    // The literal word "password" legitimately appears as an auth_methods label. What must never leak is
    // the submitted secret, a hash, or a password-like field.
    const std::string dumped = r.body.dump();
    EXPECT_EQ(dumped.find("correct horse battery"), std::string::npos);
    EXPECT_EQ(dumped.find("argon2"), std::string::npos);
    EXPECT_EQ(dumped.find("hash"), std::string::npos);
    EXPECT_FALSE(r.body["user"].contains("password"));
    EXPECT_FALSE(r.body["user"].contains("password_hash"));
    EXPECT_EQ(r.body["user"]["auth_methods"], json::array({"password"}));
    EXPECT_NE(r.setCookie.find("chronoplan_session="), std::string::npos);
    EXPECT_NE(r.setCookie.find("HttpOnly"), std::string::npos);
    EXPECT_NE(r.setCookie.find("SameSite=Lax"), std::string::npos);
    EXPECT_EQ(r.allowOrigin, kOrigin);   // CORS restricted to the configured frontend origin
}

TEST_F(Api, CorsAllowsCredentialsWithExactOriginOnPreflightAndActualResponses) {
    CallOptions pre;
    pre.contentType.clear();
    const Reply opt = call(h.app, HTTPMethod::Options, "/api/v1/auth/google", "", pre);
    EXPECT_EQ(opt.code, 204);
    EXPECT_EQ(opt.allowOrigin, kOrigin);
    EXPECT_EQ(opt.allowCredentials, "true");
    const Reply act = h.post("/api/v1/auth/login", creds("nobody@example.com", "correct horse battery"));
    EXPECT_EQ(act.allowOrigin, kOrigin);
    EXPECT_EQ(act.allowCredentials, "true");
    EXPECT_NE(act.allowOrigin, "*");
}

TEST_F(Api, RegisterRejectsDuplicatesAndBadInput) {
    EXPECT_EQ(h.post("/api/v1/auth/register", creds("a@example.com", "correct horse battery")).code, 201);
    const Reply dup = h.post("/api/v1/auth/register", creds("A@example.com", "different password!"));
    EXPECT_EQ(dup.code, 409);
    EXPECT_EQ(dup.body["error"]["code"], "email_taken");
    EXPECT_EQ(h.post("/api/v1/auth/register", creds("not-an-email", "correct horse battery")).code, 400);
    EXPECT_EQ(h.post("/api/v1/auth/register", creds("b@example.com", "short")).code, 400);
    EXPECT_EQ(h.post("/api/v1/auth/register", "{not json").code, 400);
    EXPECT_EQ(h.post("/api/v1/auth/register", json{{"email", 5}, {"password", "x"}}.dump()).code, 400);
}

TEST_F(Api, LoginWithCorrectAndIncorrectPassword) {
    signUp(h.app, "a@example.com");
    const Reply ok = h.post("/api/v1/auth/login", creds("a@example.com", "correct horse battery"));
    EXPECT_EQ(ok.code, 200);
    EXPECT_NE(ok.setCookie.find("chronoplan_session="), std::string::npos);
    const Reply bad = h.post("/api/v1/auth/login", creds("a@example.com", "wrong password!!"));
    EXPECT_EQ(bad.code, 401);
    EXPECT_EQ(bad.body["error"]["code"], "invalid_credentials");
    EXPECT_TRUE(bad.setCookie.empty());
    EXPECT_EQ(h.post("/api/v1/auth/login", creds("ghost@example.com", "correct horse battery")).code, 401);
}

TEST_F(Api, LoginIsThrottledAfterRepeatedFailures) {
    signUp(h.app, "a@example.com");
    for (int i = 0; i < h.config.maxFailedLogins; ++i) h.post("/api/v1/auth/login", creds("a@example.com", "wrong password!!"));
    EXPECT_EQ(h.post("/api/v1/auth/login", creds("a@example.com", "correct horse battery")).code, 429);
}

TEST_F(Api, SessionPersistsAcrossRequestsLikeABrowserRefresh) {
    const TestSession s = signUp(h.app, "a@example.com");
    CallOptions o; o.session = &s;
    const Reply me = h.get("/api/v1/auth/me", o);
    ASSERT_EQ(me.code, 200) << me.raw;
    EXPECT_EQ(me.body["user"]["email"], "a@example.com");
    EXPECT_EQ(me.body["csrf_token"], s.csrf);   // a reloaded page can recover its CSRF token from the cookie alone
    EXPECT_EQ(h.get("/api/v1/auth/me", o).code, 200);
}

TEST_F(Api, UnauthenticatedRequestsGet401) {
    for (const char* url : {"/api/v1/auth/me", "/api/v1/tasks", "/api/v1/events", "/api/v1/tasks/1/dependencies"})
        EXPECT_EQ(h.get(url).code, 401) << url;
    EXPECT_EQ(h.post("/api/v1/tasks", "{}").code, 401);
    EXPECT_EQ(h.post("/api/v1/schedule/optimize", "{}").code, 401);
    TestSession fake{"chronoplan_session=not-a-real-token", "x"};
    CallOptions o; o.session = &fake;
    const Reply r = h.get("/api/v1/tasks", o);
    EXPECT_EQ(r.code, 401);
    EXPECT_NE(r.setCookie.find("Max-Age=0"), std::string::npos);   // dead cookie is cleared
}

TEST_F(Api, HealthIsPublic) {
    EXPECT_EQ(h.get("/api/v1/schedule/optimize/health").code, 200);
}

TEST_F(Api, StateChangingRequestsNeedTheCsrfToken) {
    const TestSession s = signUp(h.app, "a@example.com");
    CallOptions noCsrf; noCsrf.session = &s; noCsrf.sendCsrf = false;
    const Reply missing = h.post("/api/v1/tasks", json{{"title", "t"}, {"duration_min", 30}}.dump(), noCsrf);
    EXPECT_EQ(missing.code, 403);
    EXPECT_EQ(missing.body["error"]["code"], "csrf_failed");

    TestSession wrong = s; wrong.csrf = "0000";
    CallOptions o; o.session = &wrong;
    EXPECT_EQ(h.post("/api/v1/tasks", json{{"title", "t"}, {"duration_min", 30}}.dump(), o).code, 403);

    CallOptions good; good.session = &s;
    EXPECT_EQ(h.post("/api/v1/tasks", json{{"title", "t"}, {"duration_min", 30}}.dump(), good).code, 201);
    EXPECT_EQ(h.get("/api/v1/tasks", noCsrf).code, 200);   // reads do not need it
}

TEST_F(Api, ForeignOriginsAndNonJsonSignInsAreRefused) {
    const TestSession s = signUp(h.app, "a@example.com");
    CallOptions evil; evil.session = &s; evil.origin = "https://evil.example.com";
    EXPECT_EQ(h.post("/api/v1/tasks", json{{"title", "t"}, {"duration_min", 30}}.dump(), evil).code, 403);
    CallOptions evilLogin; evilLogin.origin = "https://evil.example.com";
    EXPECT_EQ(h.post("/api/v1/auth/login", creds("a@example.com", "correct horse battery"), evilLogin).code, 403);
    CallOptions form; form.contentType = "application/x-www-form-urlencoded";
    EXPECT_EQ(h.post("/api/v1/auth/login", "email=a@example.com&password=x", form).code, 415);
}

TEST_F(Api, LogoutInvalidatesTheSessionAndClearsTheCookie) {
    const TestSession s = signUp(h.app, "a@example.com");
    CallOptions o; o.session = &s;
    CallOptions noCsrf; noCsrf.session = &s; noCsrf.sendCsrf = false;
    EXPECT_EQ(h.post("/api/v1/auth/logout", "", noCsrf).code, 403);
    const Reply out = h.post("/api/v1/auth/logout", "", o);
    EXPECT_EQ(out.code, 204);
    EXPECT_NE(out.setCookie.find("Max-Age=0"), std::string::npos);
    EXPECT_EQ(h.get("/api/v1/auth/me", o).code, 401);   // replaying the old cookie no longer works
    EXPECT_EQ(h.get("/api/v1/tasks", o).code, 401);
}

TEST_F(Api, SessionsExpire) {
    const TestSession s = signUp(h.app, "a@example.com");
    CallOptions o; o.session = &s;
    EXPECT_EQ(h.get("/api/v1/auth/me", o).code, 200);
    *h.now += h.config.sessionTtlSeconds + 1;
    EXPECT_EQ(h.get("/api/v1/auth/me", o).code, 401);
}

TEST_F(Api, GoogleSignInThroughTheApi) {
    h.google->claims = {"sub-1", "g@example.com", "Gina", true};
    const Reply r = h.post("/api/v1/auth/google", json{{"credential", "any-token"}}.dump());
    ASSERT_EQ(r.code, 201) << r.raw;
    EXPECT_EQ(r.body["user"]["auth_methods"], json::array({"google"}));
    EXPECT_NE(r.setCookie.find("HttpOnly"), std::string::npos);
    EXPECT_EQ(h.post("/api/v1/auth/google", json{{"credential", "any-token"}}.dump()).code, 200);   // second sign-in
    h.google->reject = true;
    const Reply bad = h.post("/api/v1/auth/google", json{{"credential", "forged"}}.dump());
    EXPECT_EQ(bad.code, 401);
    EXPECT_EQ(bad.body["error"]["code"], "invalid_google_token");
    EXPECT_TRUE(bad.setCookie.empty());
    EXPECT_EQ(h.post("/api/v1/auth/google", json{{"nothing", 1}}.dump()).code, 400);
}
