#include "api/Routes.hpp"
#include "api/ApiResponse.hpp"

#include "auth/AuthErrors.hpp"

namespace chronoplan::api {

repositories::UserId requireUserId(ChronoApp& app, const crow::request& req) {
    const auto& ctx = app.get_context<AuthMiddleware>(req);
    if (!ctx.user) throw auth::UnauthorizedError("authentication required");   // never reached while the middleware is active
    return ctx.user->id;
}

nlohmann::json healthPayload() {
    return {{"status", "online"},
            {"engine_version", "1.0.0-cxx20"},
            {"solver_core", "chronoplan-greedy"}};
}

void configureCors(ChronoApp& app, const std::string& origin) {
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin(origin)
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post, crow::HTTPMethod::Put,
                 crow::HTTPMethod::Delete, crow::HTTPMethod::Options)
        .headers("Content-Type", "Accept", "X-CSRF-Token")
        .allow_credentials();   // the session cookie travels cross-origin in development (needs an exact origin, not "*")
}

void registerHealthRoutes(ChronoApp& app) {
    CROW_ROUTE(app, "/api/v1/schedule/optimize/health").methods(crow::HTTPMethod::Get)([]() {
        return jsonResponse(200, healthPayload());
    });
}

void registerFallbackRoutes(ChronoApp& app) {
    // Crow pre-sets res.code to 404 (no such route) or 405 (route exists, other method).
    CROW_CATCHALL_ROUTE(app)([](const crow::request&, crow::response& res) {
        if (res.code == 405) {
            res = errorResponse(405, "method_not_allowed", "this method is not allowed for the requested URL");
        } else {
            res = errorResponse(404, "not_found", "no such endpoint");
        }
    });
}

void registerApiRoutes(ChronoApp& app, database::Database& db, auth::AuthService& auth) {
    app.get_middleware<AuthMiddleware>().configure(&auth);
    registerHealthRoutes(app);
    registerAuthRoutes(app, auth);
    registerTaskRoutes(app, db);
    registerDependencyRoutes(app, db);
    registerEventRoutes(app, db);
    registerScheduleRoutes(app, db);
    registerFallbackRoutes(app);
}

}  // namespace chronoplan::api
