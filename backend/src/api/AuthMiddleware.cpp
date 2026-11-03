#include "api/AuthMiddleware.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include "api/ApiResponse.hpp"
#include "auth/AuthErrors.hpp"
#include "auth/Cookies.hpp"

namespace chronoplan::api {

namespace {

bool isStateChanging(crow::HTTPMethod m) {
    return m == crow::HTTPMethod::Post || m == crow::HTTPMethod::Put || m == crow::HTTPMethod::Delete ||
           m == crow::HTTPMethod::Patch;
}

bool startsWithJson(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value.rfind("application/json", 0) == 0;
}

void deny(crow::response& res, int status, const char* code, const char* message) {
    res = errorResponse(status, code, message);
    res.end();
}

}  // namespace

bool AuthMiddleware::isPublicPath(const std::string& path) {
    return path == "/api/v1/auth/register" || path == "/api/v1/auth/login" || path == "/api/v1/auth/google" ||
           path == "/api/v1/schedule/optimize/health";
}

void AuthMiddleware::before_handle(crow::request& req, crow::response& res, context& ctx) {
    const std::string& path = req.url;
    if (path.rfind("/api/v1/", 0) != 0) return;                 // not an API route (the 404 fallback etc.)
    if (req.method == crow::HTTPMethod::Options) return;        // CORS preflight: answered by CORSHandler
    const bool changes = isStateChanging(req.method);

    // Origin check (defence in depth next to CSRF tokens): browsers attach Origin to every
    // cross-origin request and to same-origin non-GET fetches, so a foreign page cannot hide it.
    if (changes && service_) {
        const std::string origin = req.get_header_value("Origin");
        if (!origin.empty() && origin != service_->config().frontendOrigin)
            return deny(res, 403, "forbidden", "request origin is not allowed");
    }

    if (isPublicPath(path)) {
        // JSON only: a plain HTML form cannot send application/json, which blocks "login CSRF".
        if (req.method == crow::HTTPMethod::Post && !startsWithJson(req.get_header_value("Content-Type")))
            return deny(res, 415, "unsupported_media_type", "Content-Type must be application/json");
        return;
    }

    if (!service_) return deny(res, 401, "unauthorized", "authentication required");
    const auth::AuthConfig& config = service_->config();

    try {
        const auto token = auth::findCookie(req.get_header_value("Cookie"), config.cookieName);
        std::optional<auth::AuthenticatedUser> user;
        if (token) user = service_->authenticate(*token);
        if (!user) {
            deny(res, 401, "unauthorized", "authentication required");
            if (token) res.add_header("Set-Cookie", auth::buildClearedSessionCookie(config));   // drop the dead cookie
            return;
        }
        if (changes && !auth::AuthService::csrfMatches(*user, req.get_header_value("X-CSRF-Token")))
            return deny(res, 403, "csrf_failed", "missing or invalid CSRF token");
        ctx.user = std::move(user);
        ctx.sessionToken = *token;
    } catch (const std::exception& e) {
        std::cerr << "[auth] middleware error: " << e.what() << '\n';
        deny(res, 503, "service_unavailable", "authentication is temporarily unavailable");
    }
}

std::string clientKeyOf(const crow::request& req, const auth::AuthConfig& config) {
    if (config.trustForwardedFor) {
        const std::string xff = req.get_header_value("X-Forwarded-For");
        if (!xff.empty()) {
            const auto comma = xff.rfind(',');
            std::string last = comma == std::string::npos ? xff : xff.substr(comma + 1);
            const auto b = last.find_first_not_of(' ');
            const auto e = last.find_last_not_of(' ');
            if (b != std::string::npos) return last.substr(b, e - b + 1);
        }
    }
    return req.remote_ip_address;
}

}  // namespace chronoplan::api
