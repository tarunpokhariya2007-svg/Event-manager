#pragma once
// Helpers for the authentication tests: fake Google collaborators, an in-test RSA key (OpenSSL)
// that can sign ID tokens, and in-process Crow calls that carry the session cookie + CSRF token.
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <crow.h>
#include <nlohmann/json.hpp>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include "TestDb.hpp"
#include "api/Routes.hpp"
#include "auth/AuthErrors.hpp"
#include "auth/AuthService.hpp"
#include "auth/Crypto.hpp"
#include "auth/GoogleTokenVerifier.hpp"

namespace authtest {

inline const std::string kOrigin = "http://localhost:5173";
inline const std::string kClientId = "test-client.apps.googleusercontent.com";

inline chronoplan::auth::AuthConfig testConfig() {
    chronoplan::auth::AuthConfig c;
    c.frontendOrigin = kOrigin;
    c.googleClientId = kClientId;
    return c;
}

inline long long wallClock() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// ---- Google fakes ----------------------------------------------------------------------------
struct FakeGoogleVerifier : chronoplan::auth::IGoogleTokenVerifier {
    chronoplan::auth::GoogleClaims claims;
    bool reject = false;
    chronoplan::auth::GoogleClaims verify(const std::string&) override {
        if (reject) throw chronoplan::auth::InvalidGoogleTokenError("rejected by test");
        return claims;
    }
};

struct TestRsaKey {
    EVP_PKEY* pkey = nullptr;
    std::string modulus, exponent;   // raw big-endian bytes, as a JWK provider would return them

    explicit TestRsaKey(unsigned bits = 2048) {
        pkey = EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", static_cast<std::size_t>(bits));
        BIGNUM *n = nullptr, *e = nullptr;
        EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_N, &n);
        EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_E, &e);
        modulus.resize(BN_num_bytes(n));
        BN_bn2bin(n, reinterpret_cast<unsigned char*>(modulus.data()));
        exponent.resize(BN_num_bytes(e));
        BN_bn2bin(e, reinterpret_cast<unsigned char*>(exponent.data()));
        BN_free(n);
        BN_free(e);
    }
    ~TestRsaKey() { EVP_PKEY_free(pkey); }
    TestRsaKey(const TestRsaKey&) = delete;
    TestRsaKey& operator=(const TestRsaKey&) = delete;

    std::string sign(const std::string& message) const {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey);
        std::size_t len = 0;
        EVP_DigestSign(ctx, nullptr, &len, reinterpret_cast<const unsigned char*>(message.data()), message.size());
        std::string sig(len, '\0');
        EVP_DigestSign(ctx, reinterpret_cast<unsigned char*>(sig.data()), &len,
                       reinterpret_cast<const unsigned char*>(message.data()), message.size());
        sig.resize(len);
        EVP_MD_CTX_free(ctx);
        return sig;
    }

    // header.payload.signature with the given JSON header/payload (RS256 unless the header says otherwise).
    std::string makeJwt(const nlohmann::json& header, const nlohmann::json& payload) const {
        const std::string h = chronoplan::auth::base64UrlEncode(header.dump());
        const std::string p = chronoplan::auth::base64UrlEncode(payload.dump());
        return h + "." + p + "." + chronoplan::auth::base64UrlEncode(sign(h + "." + p));
    }
};

struct FakeJwks : chronoplan::auth::IJwksProvider {
    std::string kid = "test-kid";
    chronoplan::auth::RsaPublicKey key;
    bool unavailable = false;
    std::optional<chronoplan::auth::RsaPublicKey> findKey(const std::string& k) override {
        if (unavailable) throw chronoplan::auth::GoogleUnavailableError("down");
        if (k == kid) return key;
        return std::nullopt;
    }
};

// ---- Crow helpers ----------------------------------------------------------------------------
struct Reply {
    int code = 0;
    nlohmann::json body;
    std::string raw;
    std::string setCookie;
    std::string allowOrigin;
    std::string allowCredentials;
};

struct TestSession {
    std::string cookie;   // "chronoplan_session=<token>"
    std::string csrf;
};

struct CallOptions {
    const TestSession* session = nullptr;
    bool sendCsrf = true;
    bool sendOrigin = true;
    std::string origin = kOrigin;
    std::string contentType = "application/json";
    std::string clientIp = "127.0.0.1";
};

// crow::App::handle_full() runs only the router. It skips the GLOBAL middleware (CORS headers,
// authentication, CSRF/Origin checks) and leaves req.middleware_context null, so a handler that
// calls get_context<AuthMiddleware>() dereferences null (SEH 0xc0000005) and no Access-Control-*
// header is ever added. This reproduces what crow::Connection does for a real request (before-handlers
// in order, route, after-handlers in reverse order) using only Crow's public API. No socket or thread.
inline void dispatch(chronoplan::api::ChronoApp& app, crow::request& req, crow::response& res) {
    using chronoplan::api::AuthMiddleware;
    chronoplan::api::ChronoApp::context_t ctx;
    req.middleware_context = static_cast<void*>(&ctx);
    auto& cors = app.get_middleware<crow::CORSHandler>();
    auto& auth = app.get_middleware<AuthMiddleware>();
    auto& corsCtx = app.get_context<crow::CORSHandler>(req);
    auto& authCtx = app.get_context<AuthMiddleware>(req);
    cors.before_handle(req, res, corsCtx);
    if (!res.is_completed()) auth.before_handle(req, res, authCtx);
    if (!res.is_completed()) app.handle_full(req, res);
    auth.after_handle(req, res, authCtx);
    cors.after_handle(req, res, corsCtx);
}

inline Reply call(chronoplan::api::ChronoApp& app, crow::HTTPMethod method, const std::string& target,
                  const std::string& body = "", const CallOptions& o = {}) {
    crow::request req;
    req.method = method;
    req.raw_url = target;
    req.url = target.substr(0, target.find('?'));
    req.url_params = crow::query_string(target);
    req.body = body;
    req.remote_ip_address = o.clientIp;
    if (!o.contentType.empty()) req.headers.emplace("Content-Type", o.contentType);
    if (o.sendOrigin) req.headers.emplace("Origin", o.origin);
    if (o.session) {
        req.headers.emplace("Cookie", o.session->cookie);
        if (o.sendCsrf) req.headers.emplace("X-CSRF-Token", o.session->csrf);
    }
    crow::response res;
    dispatch(app, req, res);
    Reply r;
    r.code = res.code;
    r.raw = res.body;
    r.setCookie = res.get_header_value("Set-Cookie");
    r.allowOrigin = res.get_header_value("Access-Control-Allow-Origin");
    r.allowCredentials = res.get_header_value("Access-Control-Allow-Credentials");
    if (!res.body.empty()) r.body = nlohmann::json::parse(res.body, nullptr, false);
    return r;
}

// "chronoplan_session=abc; Path=/; ..." -> "chronoplan_session=abc"
inline std::string cookiePair(const std::string& setCookie) { return setCookie.substr(0, setCookie.find(';')); }

inline TestSession signUp(chronoplan::api::ChronoApp& app, const std::string& email,
                          const std::string& password = "correct horse battery") {
    const Reply r = call(app, crow::HTTPMethod::Post, "/api/v1/auth/register",
                         nlohmann::json{{"email", email}, {"password", password}}.dump());
    TestSession s;
    s.cookie = cookiePair(r.setCookie);
    s.csrf = r.body.value("csrf_token", "");
    return s;
}

// Everything an API test needs: temp DB, real AuthService with a controllable clock and fake Google.
struct AuthHarness {
    testutil::TempDb tmp;
    chronoplan::database::DatabaseManager mgr{tmp.config()};
    chronoplan::auth::AuthConfig config = testConfig();
    std::shared_ptr<long long> now = std::make_shared<long long>(wallClock());
    std::shared_ptr<FakeGoogleVerifier> google = std::make_shared<FakeGoogleVerifier>();
    chronoplan::auth::AuthService service{mgr.database(), config, google, [n = now] { return *n; }};
    chronoplan::api::ChronoApp app;

    AuthHarness() {
        chronoplan::api::configureCors(app, config.frontendOrigin);
        chronoplan::api::registerApiRoutes(app, mgr.database(), service);
        app.validate();
    }
    Reply get(const std::string& url, const CallOptions& o = {}) { return call(app, crow::HTTPMethod::Get, url, "", o); }
    Reply post(const std::string& url, const std::string& body, const CallOptions& o = {}) { return call(app, crow::HTTPMethod::Post, url, body, o); }
};

}  // namespace authtest
