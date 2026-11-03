#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include "api/ChronoApp.hpp"
#include "auth/AuthService.hpp"
#include "database/Database.hpp"
#include "repositories/Repositories.hpp"

namespace chronoplan::api {

// Body of GET /api/v1/schedule/optimize/health. Kept separate from the route so
// it can be unit-tested without starting a server.
nlohmann::json healthPayload();

// `origin` is the Access-Control-Allow-Origin value. "*" (the default) is for local
// development only; deployments should pass the exact frontend origin.
void configureCors(ChronoApp& app, const std::string& origin = "*");
void registerHealthRoutes(ChronoApp& app);
// The data routes build their repositories per request, bound to the authenticated user.
void registerTaskRoutes(ChronoApp& app, database::Database& db);
void registerEventRoutes(ChronoApp& app, database::Database& db);
void registerDependencyRoutes(ChronoApp& app, database::Database& db);
// JSON bodies for Crow's own 404 (unknown route) and 405 (wrong method) responses.
void registerFallbackRoutes(ChronoApp& app);
// POST /api/v1/schedule/optimize (Phase 4 scheduling engine; read-only preview).
void registerScheduleRoutes(ChronoApp& app, database::Database& db);
// POST /api/v1/auth/{register,login,google,logout}, GET /api/v1/auth/me.
void registerAuthRoutes(ChronoApp& app, auth::AuthService& auth);
// Connects the auth service to the middleware, then registers health + auth + tasks + events +
// dependencies + schedule + fallbacks (call configureCors first). Everything except health and
// the three sign-in routes requires a session.
void registerApiRoutes(ChronoApp& app, database::Database& db, auth::AuthService& auth);

}  // namespace chronoplan::api
