#include <memory>
#include "api/ApiResponse.hpp"
#include "api/RouteHelpers.hpp"
#include "api/Routes.hpp"
#include "services/SchedulingService.hpp"

namespace chronoplan::api {

// POST /api/v1/schedule/optimize: a PREVIEW. It reads tasks/events, returns a proposed schedule
// and never writes to the database. Infeasible schedules are 200 (status partial/infeasible).
void registerScheduleRoutes(ChronoApp& app, database::Database& db) {
    CROW_ROUTE(app, "/api/v1/schedule/optimize").methods(crow::HTTPMethod::Post)(
        [&app, &db](const crow::request& req) {
            return guarded([&]() {
                // Tasks/events are read for the authenticated user only (the engine itself is unchanged).
                repositories::Repositories repos(db, requireUserId(app, req));
                services::SchedulingService service(repos);
                const nlohmann::json body = parseJsonBody(req);
                return jsonResponse(200, service.optimize(body).toApiJson());
            });
        });
}

}  // namespace chronoplan::api
