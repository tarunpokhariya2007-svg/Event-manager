#pragma once
// Crow middleware: authenticates the session cookie, enforces CSRF + Origin checks on
// state-changing requests, and exposes the current user to route handlers via its context.
#include <optional>
#include <string>
#include <crow.h>
#include "auth/AuthService.hpp"

namespace chronoplan::api {

class AuthMiddleware {
public:
    struct context {
        std::optional<auth::AuthenticatedUser> user;
        std::string sessionToken;
    };

    // Until configured, every protected route answers 401 (fail closed).
    void configure(auth::AuthService* service) { service_ = service; }

    void before_handle(crow::request& req, crow::response& res, context& ctx);
    // Crow's CORSHandler omits Access-Control-Allow-Credentials on preflight (OPTIONS) responses, but
    // browsers require it there for credentialed requests. Add it only for the exact configured
    // frontend origin (never "*", never an echoed foreign origin). CORSHandler's after_handle runs after
    // this one and keeps this header (it never overrides an existing value).
    void after_handle(crow::request& req, crow::response& res, context&) {
        if (req.method != crow::HTTPMethod::Options || !service_) return;
        const std::string& allowed = service_->config().frontendOrigin;
        if (!allowed.empty() && allowed != "*" && req.get_header_value("Origin") == allowed)
            res.set_header("Access-Control-Allow-Credentials", "true");
    }

    // Routes reachable without a session.
    static bool isPublicPath(const std::string& path);

private:
    auth::AuthService* service_ = nullptr;
};

// Identifies the caller for throttling: the peer address, or (only when the operator says a single
// trusted reverse proxy is in front) the last X-Forwarded-For entry that the proxy appended.
std::string clientKeyOf(const crow::request& req, const auth::AuthConfig& config);

}  // namespace chronoplan::api
