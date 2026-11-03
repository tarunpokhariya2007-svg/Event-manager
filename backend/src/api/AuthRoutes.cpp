#include <functional>
#include <iostream>
#include "api/ApiResponse.hpp"
#include "api/RouteHelpers.hpp"
#include "api/Routes.hpp"
#include "auth/AuthErrors.hpp"
#include "auth/Cookies.hpp"
#include "models/Validation.hpp"

namespace chronoplan::api {

namespace {

using nlohmann::json;

// Auth-specific outcomes first, then the shared mapping (validation 400, database 500/503, ...).
crow::response authGuarded(const std::function<crow::response()>& handler) {
    return guarded([&]() -> crow::response {
        try {
            return handler();
        } catch (const auth::InvalidCredentialsError& e) {
            return errorResponse(401, "invalid_credentials", e.what());
        } catch (const auth::TooManyAttemptsError& e) {
            return errorResponse(429, "too_many_attempts", e.what());
        } catch (const auth::EmailTakenError& e) {
            return errorResponse(409, "email_taken", e.what());
        } catch (const auth::AccountExistsError& e) {
            return errorResponse(409, "account_exists", e.what());
        } catch (const auth::InvalidGoogleTokenError& e) {
            std::cerr << "[auth] rejected Google token: " << e.what() << '\n';
            return errorResponse(401, "invalid_google_token", "Google sign-in could not be verified");
        } catch (const auth::GoogleUnavailableError&) {
            return errorResponse(503, "google_unavailable", "Google sign-in is temporarily unavailable");
        } catch (const auth::GoogleNotConfiguredError& e) {
            return errorResponse(503, "google_not_configured", e.what());
        }
    });
}

json userJson(const auth::AuthenticatedUser& u) {
    json methods = json::array();
    if (u.hasPassword) methods.push_back("password");
    if (u.hasGoogle) methods.push_back("google");
    return {{"id", u.id}, {"email", u.email}, {"display_name", u.displayName}, {"auth_methods", methods}};
}

crow::response sessionResponse(int status, const auth::AuthService& auth, const auth::SignInResult& r) {
    json body = {{"user", userJson(r.user)}, {"csrf_token", r.user.csrfToken}};
    if (r.created) body["created"] = true;
    crow::response res = jsonResponse(status, body);
    res.add_header("Set-Cookie", auth::buildSessionCookie(auth.config(), r.sessionToken, r.maxAgeSeconds));
    res.add_header("Cache-Control", "no-store");
    return res;
}

// A required string member; anything else (missing, null, number, ...) is a 400.
std::string requireString(const json& body, const char* key) {
    const auto it = body.find(key);
    if (it == body.end() || !it->is_string()) throw models::ValidationError(std::string("'") + key + "' must be a string");
    return it->get<std::string>();
}

std::string optionalString(const json& body, const char* key) {
    const auto it = body.find(key);
    if (it == body.end() || it->is_null()) return "";
    if (!it->is_string()) throw models::ValidationError(std::string("'") + key + "' must be a string");
    return it->get<std::string>();
}

json parseObjectBody(const crow::request& req) {
    json body = parseJsonBody(req);
    if (!body.is_object()) throw models::ValidationError("request body must be a JSON object");
    return body;
}

}  // namespace

void registerAuthRoutes(ChronoApp& app, auth::AuthService& auth) {
    CROW_ROUTE(app, "/api/v1/auth/register").methods(crow::HTTPMethod::Post)([&auth](const crow::request& req) {
        return authGuarded([&]() {
            const json body = parseObjectBody(req);
            const auto result = auth.registerUser(requireString(body, "email"), requireString(body, "password"),
                                                  optionalString(body, "display_name"), clientKeyOf(req, auth.config()));
            return sessionResponse(201, auth, result);
        });
    });

    CROW_ROUTE(app, "/api/v1/auth/login").methods(crow::HTTPMethod::Post)([&auth](const crow::request& req) {
        return authGuarded([&]() {
            const json body = parseObjectBody(req);
            const auto result = auth.login(requireString(body, "email"), requireString(body, "password"),
                                           clientKeyOf(req, auth.config()));
            return sessionResponse(200, auth, result);
        });
    });

    // Body: {"credential": "<Google ID token>"} (Google Identity Services' field name; "id_token" also accepted).
    CROW_ROUTE(app, "/api/v1/auth/google").methods(crow::HTTPMethod::Post)([&auth](const crow::request& req) {
        return authGuarded([&]() {
            const json body = parseObjectBody(req);
            const std::string token = body.contains("credential") ? requireString(body, "credential") : requireString(body, "id_token");
            const auto result = auth.loginWithGoogle(token, clientKeyOf(req, auth.config()));
            return sessionResponse(result.created ? 201 : 200, auth, result);
        });
    });

    CROW_ROUTE(app, "/api/v1/auth/me").methods(crow::HTTPMethod::Get)([&app](const crow::request& req) {
        return authGuarded([&]() {
            auto& ctx = app.get_context<AuthMiddleware>(req);
            if (!ctx.user) throw auth::UnauthorizedError("authentication required");
            crow::response res = jsonResponse(200, {{"user", userJson(*ctx.user)}, {"csrf_token", ctx.user->csrfToken}});
            res.add_header("Cache-Control", "no-store");
            return res;
        });
    });

    CROW_ROUTE(app, "/api/v1/auth/logout").methods(crow::HTTPMethod::Post)([&app, &auth](const crow::request& req) {
        return authGuarded([&]() {
            auto& ctx = app.get_context<AuthMiddleware>(req);
            if (!ctx.user) throw auth::UnauthorizedError("authentication required");
            auth.logout(ctx.sessionToken);
            crow::response res = noContent();
            res.add_header("Set-Cookie", auth::buildClearedSessionCookie(auth.config()));
            res.add_header("Cache-Control", "no-store");
            return res;
        });
    });
}

}  // namespace chronoplan::api
