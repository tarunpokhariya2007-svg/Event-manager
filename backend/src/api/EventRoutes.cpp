#include "api/ApiResponse.hpp"
#include "api/RouteHelpers.hpp"
#include "api/Routes.hpp"
#include "repositories/RepositoryErrors.hpp"

namespace chronoplan::api {

using models::Event;
using models::Id;
using models::ValidationError;

namespace {

crow::response listEvents(repositories::EventRepository& events, const crow::request& req) {
    std::vector<Event> found;
    if (const char* date = req.url_params.get("date")) {
        found = events.getEventsByDate(date);   // ValidationError (400) on a bad/empty date
    } else {
        found = events.getAllEvents();
    }
    nlohmann::json arr = nlohmann::json::array();
    for (const Event& e : found) arr.push_back(e.toJson());
    return jsonResponse(200, arr);
}

crow::response getEvent(repositories::EventRepository& events, const Id& id) {
    const auto e = events.getEventById(id);
    if (!e) throw repositories::NotFoundError("event " + (id.numeric ? id.value : "\"" + id.value + "\"") + " not found");
    return jsonResponse(200, e->toJson());
}

}  // namespace

void registerEventRoutes(ChronoApp& app, database::Database& db) {
    CROW_ROUTE(app, "/api/v1/events").methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)(
        [&app, &db](const crow::request& req) {
            return guarded([&]() {
                repositories::EventRepository events(db, requireUserId(app, req));
                if (req.method == crow::HTTPMethod::Post)
                    return jsonResponse(201, events.createEvent(Event::fromJson(parseJsonBody(req))).toJson());
                return listEvents(events, req);
            });
        });

    CROW_ROUTE(app, "/api/v1/events/<string>")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Put, crow::HTTPMethod::Delete)(
            [&app, &db](const crow::request& req, const std::string& rawId) {
                return guarded([&]() {
                    repositories::EventRepository events(db, requireUserId(app, req));
                    const Id id = parseIdSegment(rawId, "event id");
                    if (req.method == crow::HTTPMethod::Put) {
                        Event e = Event::fromJson(parseJsonBody(req));
                        if (!e.id.empty() && !(e.id == id))
                            throw ValidationError("id in the body does not match the id in the URL");
                        return jsonResponse(200, events.updateEvent(id, std::move(e)).toJson());
                    }
                    if (req.method == crow::HTTPMethod::Delete) {
                        events.deleteEvent(id);
                        return noContent();
                    }
                    return getEvent(events, id);
                });
            });
}

}  // namespace chronoplan::api
