#include "api/ApiResponse.hpp"
#include "api/RouteHelpers.hpp"
#include "api/Routes.hpp"
#include "repositories/RepositoryErrors.hpp"

namespace chronoplan::api {

using models::Id;
using models::Task;
using models::ValidationError;

namespace {

crow::response listTasks(repositories::TaskRepository& tasks) {
    nlohmann::json arr = nlohmann::json::array();
    for (const Task& t : tasks.getAllTasks()) arr.push_back(t.toJson());
    return jsonResponse(200, arr);
}

crow::response createTask(repositories::TaskRepository& tasks, const crow::request& req) {
    const Task created = tasks.createTask(Task::fromJson(parseJsonBody(req)));
    return jsonResponse(201, created.toJson());
}

crow::response getTask(repositories::TaskRepository& tasks, const Id& id) {
    const auto t = tasks.getTaskById(id);
    if (!t) throw repositories::NotFoundError("task " + (id.numeric ? id.value : "\"" + id.value + "\"") + " not found");
    return jsonResponse(200, t->toJson());
}

crow::response updateTask(repositories::TaskRepository& tasks, const Id& id, const crow::request& req) {
    const nlohmann::json body = parseJsonBody(req);
    Task t = Task::fromJson(body);
    if (!t.id.empty() && !(t.id == id))
        throw ValidationError("id in the body does not match the id in the URL");
    return jsonResponse(200, tasks.updateTask(id, std::move(t)).toJson());
}

}  // namespace

void registerTaskRoutes(ChronoApp& app, database::Database& db) {
    CROW_ROUTE(app, "/api/v1/tasks").methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)(
        [&app, &db](const crow::request& req) {
            return guarded([&]() {
                repositories::TaskRepository tasks(db, requireUserId(app, req));
                return req.method == crow::HTTPMethod::Post ? createTask(tasks, req) : listTasks(tasks);
            });
        });

    CROW_ROUTE(app, "/api/v1/tasks/<string>")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Put, crow::HTTPMethod::Delete)(
            [&app, &db](const crow::request& req, const std::string& rawId) {
                return guarded([&]() {
                    repositories::TaskRepository tasks(db, requireUserId(app, req));
                    const Id id = parseIdSegment(rawId, "task id");
                    if (req.method == crow::HTTPMethod::Put) return updateTask(tasks, id, req);
                    if (req.method == crow::HTTPMethod::Delete) {
                        tasks.deleteTask(id);
                        return noContent();
                    }
                    return getTask(tasks, id);
                });
            });
}

}  // namespace chronoplan::api
