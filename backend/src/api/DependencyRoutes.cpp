#include "api/ApiResponse.hpp"
#include "api/RouteHelpers.hpp"
#include "api/Routes.hpp"

namespace chronoplan::api {

using models::Id;

namespace {

nlohmann::json dependencyJson(const Id& task, const Id& prerequisite) {
    return {{"task_id", task.toJson()}, {"prerequisite_id", prerequisite.toJson()}};
}

}  // namespace

void registerDependencyRoutes(ChronoApp& app, database::Database& db) {
    CROW_ROUTE(app, "/api/v1/tasks/<string>/dependencies")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)(
            [&app, &db](const crow::request& req, const std::string& rawId) {
                return guarded([&]() {
                    repositories::DependencyRepository deps(db, requireUserId(app, req));
                    const Id task = parseIdSegment(rawId, "task id");
                    if (req.method == crow::HTTPMethod::Post) {
                        const nlohmann::json body = parseJsonBody(req);
                        if (!body.is_object()) throw models::ValidationError("request body must be a JSON object");
                        const auto it = body.find("prerequisite_id");
                        if (it == body.end() || it->is_null())
                            throw models::ValidationError("missing required field 'prerequisite_id'");
                        const Id prerequisite = models::parseId(*it, "prerequisite_id");
                        deps.addDependency(task, prerequisite);
                        return jsonResponse(201, dependencyJson(task, prerequisite));
                    }
                    nlohmann::json arr = nlohmann::json::array();
                    for (const Id& p : deps.getPrerequisites(task)) arr.push_back(dependencyJson(task, p));
                    return jsonResponse(200, arr);
                });
            });

    CROW_ROUTE(app, "/api/v1/tasks/<string>/dependencies/<string>")
        .methods(crow::HTTPMethod::Delete)(
            [&app, &db](const crow::request& req, const std::string& rawTask, const std::string& rawPrereq) {
                return guarded([&]() {
                    repositories::DependencyRepository deps(db, requireUserId(app, req));
                    deps.removeDependency(parseIdSegment(rawTask, "task id"),
                                          parseIdSegment(rawPrereq, "prerequisite id"));
                    return noContent();
                });
            });
}

}  // namespace chronoplan::api
