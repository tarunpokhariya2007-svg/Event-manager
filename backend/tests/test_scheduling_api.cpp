// POST /api/v1/schedule/optimize through the real Crow router, against a temporary SQLite database.
#include <gtest/gtest.h>
#include "SchedTestUtil.hpp"
#include "TestDb.hpp"
#include "AuthTestUtil.hpp"
#include "api/Routes.hpp"

using namespace chronoplan;
using namespace schedtest;
using nlohmann::json;

namespace {

struct Reply { int code; json body; std::string contentType; std::string raw; };

struct ScheduleApi : ::testing::Test {
    testutil::TempDb tmp;
    database::DatabaseManager mgr{tmp.config()};
    auth::AuthConfig authConfig = authtest::testConfig();
    auth::AuthService authService{mgr.database(), authConfig, nullptr, authtest::wallClock};
    repositories::Repositories repos{mgr.database(), 1};   // the signed-up test user is the first user (id 1)
    api::ChronoApp app;
    authtest::TestSession session;

    void SetUp() override {
        api::configureCors(app, authConfig.frontendOrigin);
        api::registerApiRoutes(app, mgr.database(), authService);
        app.validate();
        session = authtest::signUp(app, "sched@example.com");
    }
    Reply call(crow::HTTPMethod m, const std::string& url, const std::string& b = "") {
        crow::request req;
        req.method = m; req.raw_url = url; req.url = url; req.body = b;
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
    Reply optimize(const json& b) { return call(crow::HTTPMethod::Post, "/api/v1/schedule/optimize", b.dump()); }
    json snapshot() {   // everything a preview could possibly touch
        json tasks = json::array(), events = json::array();
        for (const auto& t : repos.tasks.getAllTasks()) tasks.push_back(t.toJson());
        for (const auto& e : repos.events.getAllEvents()) events.push_back(e.toJson());
        return {{"tasks", tasks}, {"events", events}};
    }
};

void expectError(const Reply& r, int code, const std::string& err) {
    EXPECT_EQ(r.code, code) << r.raw;
    ASSERT_TRUE(r.body.is_object()) << r.raw;
    ASSERT_TRUE(r.body.contains("error")) << r.raw;
    EXPECT_EQ(r.body["error"]["code"], err) << r.raw;
    EXPECT_TRUE(r.body["error"]["message"].is_string());
    EXPECT_EQ(r.contentType, "application/json");
}

}  // namespace

TEST_F(ScheduleApi, ValidRequestReturnsTheStructuredSchedule) {
    const Reply r = optimize(body(cfg({{"objective", "balanced"}}), {task("t-1", 60), task("t-2", 90, {{"prerequisite_ids", json::array({"t-1"})}})},
                                  {event("e-1", H(10), H(11))}));
    ASSERT_EQ(r.code, 200) << r.raw;
    EXPECT_EQ(r.contentType, "application/json");
    EXPECT_EQ(r.body["status"], "success");
    EXPECT_EQ(r.body["target_date"], kMon);
    EXPECT_EQ(r.body["objective"], "balanced");
    EXPECT_TRUE(r.body["validation_errors"].empty());
    ASSERT_EQ(r.body["timeline"].size(), 3u);
    EXPECT_EQ(r.body["timeline"][0]["start"], "09:00");
    EXPECT_EQ(r.body["timeline"][0]["reference_id"], "t-1");
    EXPECT_EQ(r.body["timeline"][1]["status"], "fixed");
    EXPECT_EQ(r.body["timeline"][2]["reference_id"], "t-2");
    EXPECT_EQ(r.body["timeline"][2]["start"], "11:00");
    EXPECT_EQ(r.body["statistics"]["tasks_scheduled"], 2);
    EXPECT_EQ(r.body["total_allocated_min"], 150);
    EXPECT_TRUE(r.body["solver_duration_ms"].is_number());
}

TEST_F(ScheduleApi, ContractSampleStyleRequestWorks) {
    // Phase 1 shape: single date, work window fields, no Phase 4 fields at all.
    const json b = {{"version", "1.0"},
                    {"config", {{"target_date", "2026-10-31"}, {"work_window_start_min", 510}, {"work_window_end_min", 1110},
                                {"min_break_min", 30}, {"allow_splitting", true}, {"max_workload_min", 450}, {"objective", "balanced"}}},
                    {"fixed_events", json::array({event("e-1", 600, 660, "2026-10-31")})},
                    {"flexible_tasks", json::array({task("t-1", 60, {{"allow_split", true}}), task("t-2", 90, {{"priority", 4}, {"prerequisite_ids", json::array({"t-1"})}})})}};
    const Reply r = optimize(b);
    ASSERT_EQ(r.code, 200) << r.raw;
    EXPECT_EQ(r.body["status"], "success");
    EXPECT_EQ(r.body["total_allocated_min"], 150);
}

TEST_F(ScheduleApi, EmptyBodyAndInvalidJsonAre400InvalidJson) {
    expectError(call(crow::HTTPMethod::Post, "/api/v1/schedule/optimize", ""), 400, "invalid_json");
    expectError(call(crow::HTTPMethod::Post, "/api/v1/schedule/optimize", "{not json"), 400, "invalid_json");
}

TEST_F(ScheduleApi, EmptyObjectAndNonObjectBodiesAreValidationErrors) {
    expectError(optimize(json::object()), 400, "validation_error");
    expectError(optimize(json::array()), 400, "validation_error");
    expectError(optimize(json("text")), 400, "validation_error");
}

TEST_F(ScheduleApi, InvalidDateRangesAreRejected) {
    expectError(optimize(body(cfg({{"end_date", "2026-11-01"}}))), 400, "validation_error");
    expectError(optimize(body(cfg({{"target_date", "2026-13-01"}}))), 400, "validation_error");
    expectError(optimize(body(cfg({{"end_date", "garbage"}}))), 400, "validation_error");
    expectError(optimize(body(cfg({{"end_date", "2030-01-01"}}))), 400, "validation_error");
}

TEST_F(ScheduleApi, InvalidAvailabilityIsRejected) {
    auto win = [](int a, int b) { return json({{"start_min", a}, {"end_min", b}}); };
    expectError(optimize(body(cfg({{"availability_windows", json::array({win(700, 600)})}}))), 400, "validation_error");
    expectError(optimize(body(cfg({{"availability_windows", json::array({win(540, 700), win(600, 800)})}}))), 400, "validation_error");
    expectError(optimize(body(cfg({{"availability_windows", "all day"}}))), 400, "validation_error");
    expectError(optimize(body(cfg({{"work_window_start_min", 900}, {"work_window_end_min", 800}}))), 400, "validation_error");
    expectError(optimize(body(cfg({{"objective", "random"}}))), 400, "validation_error");
}

TEST_F(ScheduleApi, MissingOrInvalidTaskDataIsRejected) {
    expectError(optimize(body(cfg(), {json({{"id", "a"}, {"duration_min", 30}})})), 400, "validation_error");          // no title
    expectError(optimize(body(cfg(), {json({{"id", "a"}, {"title", "x"}})})), 400, "validation_error");                // no duration
    expectError(optimize(body(cfg(), {task("a", -30)})), 400, "validation_error");                                      // negative
    expectError(optimize(body(cfg(), {task("a", 0)})), 400, "validation_error");
    expectError(optimize(body(cfg(), {task("a", 60, {{"earliest_start_min", 900}, {"latest_completion_min", 800}})})), 400, "validation_error");
    expectError(optimize(body(cfg(), {task("a", 60), task("a", 30)})), 400, "validation_error");                        // duplicate id
    expectError(optimize(body(cfg(), {task("a", 60, {{"prerequisite_ids", json::array({"ghost"})}})})), 400, "validation_error");   // unknown prerequisite
    expectError(optimize(body(cfg(), {task("a", 60, {{"deadline_date", "2026-02-30"}})})), 400, "validation_error");
    expectError(optimize(json({{"config", cfg()}, {"flexible_tasks", "nope"}})), 400, "validation_error");
    expectError(optimize(json({{"config", cfg()}, {"fixed_events", json::array({event("e", H(11), H(10))})}})), 400, "validation_error");
    expectError(optimize(json({{"flexible_tasks", json::array()}})), 400, "validation_error");                             // no config
}

TEST_F(ScheduleApi, DependencyCycleIs409CircularDependency) {
    const Reply r = optimize(body(cfg(), {task("a", 30, {{"prerequisite_ids", json::array({"b"})}}),
                                          task("b", 30, {{"prerequisite_ids", json::array({"a"})}}), task("c", 30)}));
    expectError(r, 409, "circular_dependency");
    EXPECT_NE(r.body["error"]["message"].get<std::string>().find("cycle"), std::string::npos);
}

TEST_F(ScheduleApi, InfeasibleScheduleIs200WithStatusAndReasons) {
    const Reply r = optimize(body(cfg(), {task("big", 900)}));   // one 900-minute block never fits a 480-minute day
    ASSERT_EQ(r.code, 200) << r.raw;
    EXPECT_EQ(r.body["status"], "infeasible");
    ASSERT_EQ(r.body["unscheduled_tasks"].size(), 1u);
    EXPECT_EQ(r.body["unscheduled_tasks"][0]["task_id"], "big");
    EXPECT_EQ(r.body["unscheduled_tasks"][0]["reason_code"], "no_feasible_window");
    EXPECT_FALSE(r.body["unscheduled_tasks"][0]["reason"].get<std::string>().empty());
    EXPECT_FALSE(r.body["unscheduled_tasks"][0]["suggested_action"].get<std::string>().empty());
    EXPECT_EQ(r.body["unscheduled_tasks"][0]["remaining_min"], 900);
}

TEST_F(ScheduleApi, PartialScheduleReportsRiskDetails) {
    const Reply r = optimize(body(cfg(), {task("late", 60, {{"deadline_date", "2026-11-01"}}), task("ok", 60)}));
    ASSERT_EQ(r.code, 200) << r.raw;
    EXPECT_EQ(r.body["status"], "partial");
    ASSERT_EQ(r.body["tasks_at_risk"].size(), 1u);
    const json& risk = r.body["tasks_at_risk"][0];
    EXPECT_EQ(risk["risk_level"], "impossible_without_rescheduling");
    EXPECT_EQ(risk["category"], "hard_infeasible");
    EXPECT_EQ(risk["deadline"], "2026-11-01T24:00");
    EXPECT_TRUE(risk["estimated_completion"].is_null());
}

TEST_F(ScheduleApi, MultiDayRequestReturnsDatesAndSessions) {
    const Reply r = optimize(body(cfg({{"end_date", "2026-11-03"}}), {task("a", 600, {{"allow_split", true}})}));
    ASSERT_EQ(r.code, 200) << r.raw;
    ASSERT_EQ(r.body["timeline"].size(), 2u);
    EXPECT_EQ(r.body["timeline"][0]["date"], "2026-11-02");
    EXPECT_EQ(r.body["timeline"][1]["date"], "2026-11-03");
    EXPECT_EQ(r.body["timeline"][1]["session_index"], 2);
    EXPECT_EQ(r.body["timeline"][1]["session_count"], 2);
    EXPECT_EQ(r.body["end_date"], "2026-11-03");
}

TEST_F(ScheduleApi, WrongMethodIs405) {
    expectError(call(crow::HTTPMethod::Get, "/api/v1/schedule/optimize"), 405, "method_not_allowed");
}

TEST_F(ScheduleApi, PreviewDoesNotModifyTheDatabase) {
    repos.tasks.createTask(testutil::makeTask(1, "Stored A", {{"duration_min", 60}}));
    repos.tasks.createTask(testutil::makeTask(2, "Stored B", {{"duration_min", 45}, {"prerequisite_ids", json::array({1})}}));
    repos.events.createEvent(testutil::makeEvent(1, kMon, H(9), H(10)));
    const json before = snapshot();

    // request mode (inline data) and database mode, repeated
    EXPECT_EQ(optimize(body(cfg(), {task("x", 60)}, {event("ex", H(12), H(13))})).code, 200);
    json db = body(cfg()); db["data_source"] = "database";
    db.erase("flexible_tasks"); db.erase("fixed_events");
    EXPECT_EQ(optimize(db).code, 200);
    EXPECT_EQ(optimize(db).code, 200);
    EXPECT_EQ(snapshot(), before);
    EXPECT_EQ(repos.tasks.getAllTasks().size(), 2u);
}

TEST_F(ScheduleApi, DatabaseModeSchedulesStoredTasksAroundStoredEvents) {
    repos.tasks.createTask(testutil::makeTask(1, "Stored A", {{"duration_min", 60}, {"allow_split", false}}));
    repos.tasks.createTask(testutil::makeTask(2, "Stored B", {{"duration_min", 45}, {"allow_split", false}, {"prerequisite_ids", json::array({1})}}));
    repos.events.createEvent(testutil::makeEvent(1, kMon, H(9), H(10)));
    repos.events.createEvent(testutil::makeEvent(2, "2026-11-20", H(9), H(10)));   // outside the horizon
    json b = {{"data_source", "database"}, {"config", cfg()}};
    const Reply r = optimize(b);
    ASSERT_EQ(r.code, 200) << r.raw;
    ASSERT_EQ(r.body["timeline"].size(), 3u);
    EXPECT_EQ(r.body["timeline"][0]["status"], "fixed");
    EXPECT_EQ(r.body["timeline"][1]["reference_id"], 1);              // numeric ids echoed as numbers
    EXPECT_EQ(r.body["timeline"][1]["start"], "10:00");
    EXPECT_EQ(r.body["timeline"][2]["reference_id"], 2);
    EXPECT_EQ(r.body["timeline"][2]["start"], "11:00");
}

TEST_F(ScheduleApi, DatabaseModeWithNoStoredTasksIsAnEmptySuccess) {
    const Reply r = optimize({{"data_source", "database"}, {"config", cfg()}});
    ASSERT_EQ(r.code, 200) << r.raw;
    EXPECT_EQ(r.body["status"], "success");
    EXPECT_TRUE(r.body["timeline"].empty());
}

TEST_F(ScheduleApi, MixingSourcesOrUnknownSourceIsRejected) {
    expectError(optimize({{"data_source", "database"}, {"config", cfg()}, {"flexible_tasks", json::array({task("a", 30)})}}), 400, "validation_error");
    expectError(optimize({{"data_source", "cloud"}, {"config", cfg()}}), 400, "validation_error");
    expectError(optimize({{"data_source", 5}, {"config", cfg()}}), 400, "validation_error");
}

TEST_F(ScheduleApi, StringAndNumericIdsStayDistinct) {
    const Reply r = optimize(body(cfg(), {task(7, 30), task("7", 30)}));
    ASSERT_EQ(r.code, 200) << r.raw;
    int numeric = 0, string = 0;
    for (const auto& e : r.body["timeline"]) (e["reference_id"].is_number() ? numeric : string)++;
    EXPECT_EQ(numeric, 1);
    EXPECT_EQ(string, 1);
}

TEST_F(ScheduleApi, HealthAndTaskRoutesStillWork) {
    EXPECT_EQ(call(crow::HTTPMethod::Get, "/api/v1/schedule/optimize/health").code, 200);
    EXPECT_EQ(call(crow::HTTPMethod::Get, "/api/v1/tasks").code, 200);
    expectError(call(crow::HTTPMethod::Get, "/api/v1/nothing"), 404, "not_found");
}
