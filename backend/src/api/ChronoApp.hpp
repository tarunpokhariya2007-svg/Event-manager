#pragma once
#include <crow.h>
#include <crow/middlewares/cors.h>
#include "api/AuthMiddleware.hpp"

namespace chronoplan::api {
// One app type shared by main() and the route registration functions.
// CORSHandler is needed because the Vite dev server (another origin) calls us;
// AuthMiddleware (runs after CORS) authenticates every /api/v1 request except the public ones.
using ChronoApp = crow::App<crow::CORSHandler, AuthMiddleware>;

// The authenticated user of this request. Every data route calls this before touching a repository.
// Throws auth::UnauthorizedError (-> 401) if the middleware did not authenticate the request.
repositories::UserId requireUserId(ChronoApp& app, const crow::request& req);
}  // namespace chronoplan::api
