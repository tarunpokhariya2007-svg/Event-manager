// Drives the real Crow router (routes, middleware, error mapping) in-process
// against a temporary SQLite database.
#include <gtest/gtest.h>
#include "TestDb.hpp"
#include "AuthTestUtil.hpp"
#include "api/Routes.hpp"

using namespace chronoplan;
using nlohmann::json;

namespace {

struct Reply {
    int code;
    json body;
    std::string contentType;
    std::string raw;
};

struct Api : ::testing::Test {
    testutil::TempDb tmp;
    database::DatabaseManager mgr{tmp.config()};
    auth::AuthConfig authConfig = authtest::testConfig();
    auth::AuthService authService{mgr.database(), authConfig, nullptr, authtest::wallClock};
    api::ChronoApp app;
    authtest::TestSession session;

    void SetUp() override {
        api::configureCors(app, authConfig.frontendOrigin);
        api::registerApiRoutes(app, mgr.database(), authService);
        app.validate();
        session = authtest::signUp(app, "crud@example.com");
    }

    Reply call(crow::HTTPMethod method, const std::string& target, const std::string& body = "") {
        crow::request req;
        req.method = method;
        req.raw_url = target;
        req.url = target.substr(0, target.find('?'));
        req.url_params = crow::query_string(target);
        req.body = body;
        req.headers.emplace("Content-Type", "application/json");
        req.headers.emplace("Origin", authtest::kOrigin);
        req.headers.emplace("Cookie", session.cookie);
        req.headers.emplace("X-CSRF-Token", session.csrf);
        crow::response res;
        authtest::dispatch(app, req, res);
        Reply r{res.code, json(), res.get_header_value("Content-Type"), res.body};
        if (!res.body.empty()) r.body = json::parse(res.body, nullptr, false);
        return r;
    }
    Reply get(const std::string& t) { return call(crow::HTTPMethod::Get, t); }
    Reply post(const std::string& t, const json& b) { return call(crow::HTTPMethod::Post, t, b.dump()); }
    Reply put(const std::string& t, const json& b) { return call(crow::HTTPMethod::Put, t, b.dump()); }
    Reply del(const std::string& t) { return call(crow::HTTPMethod::Delete, t); }
};

json taskBody(const json& id, const std::string& title = "Write report") {
    json j = {{"title", title}, {"duration_min", 90}, {"priority", 4}};
    if (!id.is_null()) j["id"] = id;
    return j;
}
json eventBody(const json& id, const std::string& date = "2026-11-02") {
    json j = {{"title", "Standup"}, {"date", date}, {"start_min", 540}, {"end_min", 570}};
    if (!id.is_null()) j["id"] = id;
    return j;
}

void expectError(const Reply& r, int code, const std::string& errCode) {
    EXPECT_EQ(r.code, code) << r.raw;
    ASSERT_TRUE(r.body.is_object()) << r.raw;
    ASSERT_TRUE(r.body.contains("error")) << r.raw;
    EXPECT_EQ(r.body["error"]["code"], errCode) << r.raw;
    EXPECT_TRUE(r.body["error"]["message"].is_string());
    EXPECT_EQ(r.contentType, "application/json");
}

}  // namespace

// ------------------------------------------------------------------- tasks

TEST_F(Api, HealthStillWorks) {
    const Reply r = get("/api/v1/schedule/optimize/health");
    EXPECT_EQ(r.code, 200);
    EXPECT_EQ(r.body["status"], "online");
}

TEST_F(Api, CreateTaskReturns201AndTheTask) {
    const Reply r = post("/api/v1/tasks", taskBody(5));
    EXPECT_EQ(r.code, 201) << r.raw;
    EXPECT_EQ(r.contentType, "application/json");
    EXPECT_EQ(r.body["id"], 5);
    EXPECT_TRUE(r.body["id"].is_number_integer());
    EXPECT_EQ(r.body["title"], "Write report");
    EXPECT_EQ(r.body["priority"], 4);
    EXPECT_TRUE(r.body["description"].is_null());
    EXPECT_EQ(r.body["prerequisite_ids"], json::array());
}

TEST_F(Api, CreateTaskAssignsIdWhenOmitted) {
    const Reply a = post("/api/v1/tasks", taskBody(nullptr));
    const Reply b = post("/api/v1/tasks", taskBody(nullptr));
    EXPECT_EQ(a.body["id"], 1);
    EXPECT_EQ(b.body["id"], 2);
}

TEST_F(Api, GetTaskAndList) {
    post("/api/v1/tasks", taskBody(1, "one"));
    post("/api/v1/tasks", taskBody("two", "two"));
    const Reply one = get("/api/v1/tasks/1");
    EXPECT_EQ(one.code, 200);
    EXPECT_EQ(one.body["title"], "one");
    const Reply two = get("/api/v1/tasks/two");
    EXPECT_EQ(two.code, 200);
    EXPECT_EQ(two.body["id"], "two");

    const Reply all = get("/api/v1/tasks");
    EXPECT_EQ(all.code, 200);
    ASSERT_TRUE(all.body.is_array());
    EXPECT_EQ(all.body.size(), 2u);
}

TEST_F(Api, ListIsEmptyArrayWhenNoTasks) {
    const Reply r = get("/api/v1/tasks");
    EXPECT_EQ(r.code, 200);
    EXPECT_EQ(r.body, json::array());
}

TEST_F(Api, UpdateTaskReplacesIt) {
    post("/api/v1/tasks", json{{"id", 1}, {"title", "old"}, {"duration_min", 60}, {"description", "gone soon"}});
    const Reply r = put("/api/v1/tasks/1", taskBody(nullptr, "new"));
    EXPECT_EQ(r.code, 200) << r.raw;
    EXPECT_EQ(r.body["id"], 1);
    EXPECT_EQ(r.body["title"], "new");
    EXPECT_TRUE(r.body["description"].is_null());
    EXPECT_EQ(get("/api/v1/tasks/1").body["title"], "new");
}

TEST_F(Api, UpdateTaskWithMatchingBodyIdWorksButMismatchIsRejected) {
    post("/api/v1/tasks", taskBody(1));
    EXPECT_EQ(put("/api/v1/tasks/1", taskBody(1, "same id")).code, 200);
    expectError(put("/api/v1/tasks/1", taskBody(2, "other")), 400, "validation_error");
    expectError(put("/api/v1/tasks/1", taskBody("1", "string 1 is not number 1")), 400, "validation_error");
}

TEST_F(Api, DeleteTaskReturns204ThenNotFound) {
    post("/api/v1/tasks", taskBody(1));
    const Reply r = del("/api/v1/tasks/1");
    EXPECT_EQ(r.code, 204);
    EXPECT_TRUE(r.raw.empty());
    expectError(get("/api/v1/tasks/1"), 404, "not_found");
    expectError(del("/api/v1/tasks/1"), 404, "not_found");
}

TEST_F(Api, MissingTaskIs404ForGetPutDelete) {
    expectError(get("/api/v1/tasks/99"), 404, "not_found");
    expectError(put("/api/v1/tasks/99", taskBody(nullptr)), 404, "not_found");
    expectError(del("/api/v1/tasks/99"), 404, "not_found");
}

TEST_F(Api, DuplicateTaskIdIs409) {
    post("/api/v1/tasks", taskBody(1));
    expectError(post("/api/v1/tasks", taskBody(1, "again")), 409, "conflict");
    EXPECT_EQ(get("/api/v1/tasks").body.size(), 1u);
}

TEST_F(Api, NumericAndStringIdsAreDistinctOverHttp) {
    EXPECT_EQ(post("/api/v1/tasks", taskBody(7, "number")).code, 201);
    EXPECT_EQ(post("/api/v1/tasks", taskBody("7", "string")).code, 201);   // coexists
    expectError(post("/api/v1/tasks", taskBody("7", "dup")), 409, "conflict");

    EXPECT_EQ(get("/api/v1/tasks/7").body["title"], "number");
    EXPECT_TRUE(get("/api/v1/tasks/7").body["id"].is_number_integer());
    // string id "7" is addressed with a JSON-quoted segment: %22 7 %22
    const Reply s = get("/api/v1/tasks/%227%22");
    EXPECT_EQ(s.code, 200) << s.raw;
    EXPECT_EQ(s.body["title"], "string");
    EXPECT_TRUE(s.body["id"].is_string());

    EXPECT_EQ(del("/api/v1/tasks/%227%22").code, 204);
    EXPECT_EQ(get("/api/v1/tasks/7").code, 200);          // numeric twin untouched
    EXPECT_EQ(get("/api/v1/tasks/%227%22").code, 404);
}

TEST_F(Api, InvalidTaskRequests) {
    expectError(call(crow::HTTPMethod::Post, "/api/v1/tasks", "{not json"), 400, "invalid_json");
    expectError(call(crow::HTTPMethod::Post, "/api/v1/tasks", ""), 400, "invalid_json");
    expectError(post("/api/v1/tasks", json::array()), 400, "validation_error");
    expectError(post("/api/v1/tasks", json{{"duration_min", 60}}), 400, "validation_error");          // no title
    expectError(post("/api/v1/tasks", json{{"title", "x"}, {"duration_min", 0}}), 400, "validation_error");
    expectError(post("/api/v1/tasks", json{{"title", "x"}, {"duration_min", 60}, {"id", 1.5}}), 400, "validation_error");
    expectError(call(crow::HTTPMethod::Put, "/api/v1/tasks/1", "oops"), 400, "invalid_json");
    EXPECT_EQ(get("/api/v1/tasks").body.size(), 0u);
}

TEST_F(Api, InvalidIdInUrlIs400) {
    expectError(get("/api/v1/tasks/bad%20id"), 400, "validation_error");   // space is not allowed
    expectError(get("/api/v1/tasks/%ZZ"), 400, "validation_error");
    expectError(get("/api/v1/tasks/99999999999999999999"), 400, "validation_error");
}

TEST_F(Api, TaskWithPrerequisiteIdsIsStoredAndReturned) {
    post("/api/v1/tasks", taskBody(1, "first"));
    json b = taskBody(2, "second");
    b["prerequisite_ids"] = json::array({1});
    const Reply r = post("/api/v1/tasks", b);
    EXPECT_EQ(r.code, 201) << r.raw;
    EXPECT_EQ(get("/api/v1/tasks/2").body["prerequisite_ids"], json::array({1}));

    b = taskBody(3, "third");
    b["prerequisite_ids"] = json::array({404});
    expectError(post("/api/v1/tasks", b), 400, "validation_error");
    EXPECT_EQ(get("/api/v1/tasks/3").code, 404);   // nothing was stored
}

// ------------------------------------------------------------------ events

TEST_F(Api, EventCrudLifecycle) {
    const Reply created = post("/api/v1/events", eventBody(1));
    EXPECT_EQ(created.code, 201) << created.raw;
    EXPECT_EQ(created.body["id"], 1);
    EXPECT_EQ(created.body["is_flexible"], false);
    EXPECT_EQ(created.body["category"], "");

    const Reply got = get("/api/v1/events/1");
    EXPECT_EQ(got.code, 200);
    EXPECT_EQ(got.body["title"], "Standup");

    json upd = eventBody(nullptr);
    upd["title"] = "Retro";
    upd["is_flexible"] = true;
    upd["location"] = "Room 2";
    const Reply updated = put("/api/v1/events/1", upd);
    EXPECT_EQ(updated.code, 200) << updated.raw;
    EXPECT_EQ(updated.body["title"], "Retro");
    EXPECT_EQ(get("/api/v1/events/1").body["location"], "Room 2");

    EXPECT_EQ(get("/api/v1/events").body.size(), 1u);
    EXPECT_EQ(del("/api/v1/events/1").code, 204);
    expectError(get("/api/v1/events/1"), 404, "not_found");
}

TEST_F(Api, EventErrors) {
    expectError(get("/api/v1/events/5"), 404, "not_found");
    expectError(put("/api/v1/events/5", eventBody(nullptr)), 404, "not_found");
    expectError(del("/api/v1/events/5"), 404, "not_found");
    post("/api/v1/events", eventBody(1));
    expectError(post("/api/v1/events", eventBody(1)), 409, "conflict");
    EXPECT_EQ(post("/api/v1/events", eventBody("1")).code, 201);   // string "1" differs from 1
    json bad = eventBody(2);
    bad["end_min"] = 100;                                          // before start
    expectError(post("/api/v1/events", bad), 400, "validation_error");
    expectError(post("/api/v1/events", json{{"title", "x"}}), 400, "validation_error");
    expectError(call(crow::HTTPMethod::Post, "/api/v1/events", "[["), 400, "invalid_json");
    expectError(put("/api/v1/events/1", eventBody(9)), 400, "validation_error");   // body id != URL id
}

TEST_F(Api, FilterEventsByDate) {
    post("/api/v1/events", eventBody(1, "2026-11-02"));
    post("/api/v1/events", eventBody(2, "2026-11-02"));
    post("/api/v1/events", eventBody(3, "2026-11-03"));
    const Reply r = get("/api/v1/events?date=2026-11-02");
    EXPECT_EQ(r.code, 200);
    EXPECT_EQ(r.body.size(), 2u);
    EXPECT_EQ(get("/api/v1/events?date=2030-01-01").body, json::array());
    EXPECT_EQ(get("/api/v1/events").body.size(), 3u);
    expectError(get("/api/v1/events?date=2026-02-30"), 400, "validation_error");
    expectError(get("/api/v1/events?date=tomorrow"), 400, "validation_error");
    expectError(get("/api/v1/events?date="), 400, "validation_error");
}

// ------------------------------------------------------------ dependencies

TEST_F(Api, DependencyLifecycle) {
    post("/api/v1/tasks", taskBody(1, "a"));
    post("/api/v1/tasks", taskBody(2, "b"));
    post("/api/v1/tasks", taskBody("c", "c"));

    const Reply added = post("/api/v1/tasks/2/dependencies", json{{"prerequisite_id", 1}});
    EXPECT_EQ(added.code, 201) << added.raw;
    EXPECT_EQ(added.body, (json{{"task_id", 2}, {"prerequisite_id", 1}}));
    EXPECT_EQ(post("/api/v1/tasks/c/dependencies", json{{"prerequisite_id", 2}}).code, 201);

    const Reply list = get("/api/v1/tasks/c/dependencies");
    EXPECT_EQ(list.code, 200);
    EXPECT_EQ(list.body, json::array({json{{"task_id", "c"}, {"prerequisite_id", 2}}}));
    EXPECT_EQ(get("/api/v1/tasks/1/dependencies").body, json::array());
    EXPECT_EQ(get("/api/v1/tasks/2").body["prerequisite_ids"], json::array({1}));

    const Reply removed = del("/api/v1/tasks/2/dependencies/1");
    EXPECT_EQ(removed.code, 204);
    EXPECT_EQ(get("/api/v1/tasks/2/dependencies").body, json::array());
    expectError(del("/api/v1/tasks/2/dependencies/1"), 404, "not_found");
}

TEST_F(Api, DependencyErrors) {
    post("/api/v1/tasks", taskBody(1, "a"));
    post("/api/v1/tasks", taskBody(2, "b"));
    post("/api/v1/tasks", taskBody(3, "c"));

    expectError(post("/api/v1/tasks/1/dependencies", json{{"prerequisite_id", 1}}), 400, "validation_error");  // self
    expectError(post("/api/v1/tasks/1/dependencies", json::object()), 400, "validation_error");
    expectError(post("/api/v1/tasks/1/dependencies", json{{"prerequisite_id", 1.5}}), 400, "validation_error");
    expectError(call(crow::HTTPMethod::Post, "/api/v1/tasks/1/dependencies", "nope"), 400, "invalid_json");
    expectError(post("/api/v1/tasks/99/dependencies", json{{"prerequisite_id", 1}}), 404, "not_found");
    expectError(post("/api/v1/tasks/1/dependencies", json{{"prerequisite_id", 99}}), 404, "not_found");
    expectError(get("/api/v1/tasks/99/dependencies"), 404, "not_found");
    expectError(post("/api/v1/tasks/1/dependencies", json{{"prerequisite_id", "2"}}), 404, "not_found");  // "2" != 2

    EXPECT_EQ(post("/api/v1/tasks/2/dependencies", json{{"prerequisite_id", 1}}).code, 201);
    expectError(post("/api/v1/tasks/2/dependencies", json{{"prerequisite_id", 1}}), 409, "conflict");     // duplicate
    EXPECT_EQ(post("/api/v1/tasks/3/dependencies", json{{"prerequisite_id", 2}}).code, 201);
    expectError(post("/api/v1/tasks/1/dependencies", json{{"prerequisite_id", 2}}), 409, "circular_dependency");
    expectError(post("/api/v1/tasks/1/dependencies", json{{"prerequisite_id", 3}}), 409, "circular_dependency");
    EXPECT_EQ(get("/api/v1/tasks/1/dependencies").body, json::array());   // nothing partially written
}

TEST_F(Api, DeletingATaskRemovesItsDependencies) {
    post("/api/v1/tasks", taskBody(1, "a"));
    post("/api/v1/tasks", taskBody(2, "b"));
    post("/api/v1/tasks/2/dependencies", json{{"prerequisite_id", 1}});
    EXPECT_EQ(del("/api/v1/tasks/1").code, 204);
    EXPECT_EQ(get("/api/v1/tasks/2/dependencies").body, json::array());
    EXPECT_EQ(get("/api/v1/tasks/2").body["prerequisite_ids"], json::array());
}

TEST_F(Api, UpdatingPrerequisitesThroughPutIsAtomic) {
    post("/api/v1/tasks", taskBody(1, "a"));
    json b = taskBody(2, "b");
    b["prerequisite_ids"] = json::array({1});
    post("/api/v1/tasks", b);
    json cyc = taskBody(nullptr, "renamed");
    cyc["prerequisite_ids"] = json::array({2});
    expectError(put("/api/v1/tasks/1", cyc), 409, "circular_dependency");
    EXPECT_EQ(get("/api/v1/tasks/1").body["title"], "a");   // rolled back
}

// ---------------------------------------------------- 404 / 405 / CORS JSON

TEST_F(Api, UnknownRouteReturnsJson404) {
    expectError(get("/api/v1/nope"), 404, "not_found");
    expectError(get("/"), 404, "not_found");
    expectError(get("/api/v1/tasks/1/unknown"), 404, "not_found");
}

TEST_F(Api, WrongMethodReturnsJson405) {
    expectError(call(crow::HTTPMethod::Delete, "/api/v1/tasks"), 405, "method_not_allowed");
    expectError(call(crow::HTTPMethod::Patch, "/api/v1/tasks/1"), 405, "method_not_allowed");
    expectError(call(crow::HTTPMethod::Put, "/api/v1/events"), 405, "method_not_allowed");
    expectError(call(crow::HTTPMethod::Post, "/api/v1/tasks/1/dependencies/2"), 405, "method_not_allowed");
}

TEST_F(Api, CorsPreflightStillWorks) {
    const Reply r = call(crow::HTTPMethod::Options, "/api/v1/tasks");
    EXPECT_LT(r.code, 300);
}

TEST_F(Api, ErrorsNeverLeakInternals) {
    post("/api/v1/tasks", taskBody(1));
    mgr.database().exec("DROP TABLE task_dependencies");   // simulate a broken database
    const Reply r = get("/api/v1/tasks");
    EXPECT_EQ(r.code, 500);
    EXPECT_EQ(r.body["error"]["code"], "internal_error");
    EXPECT_EQ(r.raw.find("task_dependencies"), std::string::npos);
    EXPECT_EQ(r.raw.find("sqlite"), std::string::npos);
}
