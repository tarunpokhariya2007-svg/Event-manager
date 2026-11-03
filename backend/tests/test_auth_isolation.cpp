// User A must never read, change or delete user B's tasks, events or dependencies,
// at the repository level and through the HTTP API.
#include <gtest/gtest.h>
#include "AuthTestUtil.hpp"
#include "SchedTestUtil.hpp"
#include "repositories/Repositories.hpp"
#include "repositories/RepositoryErrors.hpp"

using namespace authtest;
using namespace chronoplan;
using nlohmann::json;
using crow::HTTPMethod;
using testutil::makeEvent;
using testutil::makeTask;
using testutil::num;
using testutil::str;

namespace {
struct RepoFixture : ::testing::Test {
    testutil::TempDb tmp;
    database::DatabaseManager mgr{tmp.config()};
    long long a = testutil::makeUser(mgr.database(), "a@example.com");
    long long b = testutil::makeUser(mgr.database(), "b@example.com");
    repositories::Repositories ra{mgr.database(), a};
    repositories::Repositories rb{mgr.database(), b};
};
}  // namespace

TEST_F(RepoFixture, TasksAreInvisibleAndImmutableAcrossUsers) {
    ra.tasks.createTask(makeTask(1, "A's task"));
    EXPECT_TRUE(rb.tasks.getAllTasks().empty());
    EXPECT_FALSE(rb.tasks.getTaskById(num(1)));
    EXPECT_FALSE(rb.tasks.taskExists(num(1)));
    EXPECT_THROW(rb.tasks.updateTask(num(1), makeTask(1, "hijacked")), repositories::NotFoundError);
    EXPECT_THROW(rb.tasks.deleteTask(num(1)), repositories::NotFoundError);
    EXPECT_EQ(ra.tasks.getTaskById(num(1))->title, "A's task");
}

TEST_F(RepoFixture, IdsAreIndependentPerUser) {
    ra.tasks.createTask(makeTask(5, "A5"));
    EXPECT_NO_THROW(rb.tasks.createTask(makeTask(5, "B5")));   // same id, different owner: no clash, no leak
    EXPECT_EQ(ra.tasks.getTaskById(num(5))->title, "A5");
    EXPECT_EQ(rb.tasks.getTaskById(num(5))->title, "B5");
    EXPECT_EQ(ra.tasks.createTask(makeTask(json(nullptr), "next")).id, num(6));
    EXPECT_EQ(rb.tasks.createTask(makeTask(json(nullptr), "next")).id, num(6));
}

TEST_F(RepoFixture, EventsAreInvisibleAndImmutableAcrossUsers) {
    ra.events.createEvent(makeEvent(1, "2026-11-02"));
    EXPECT_TRUE(rb.events.getAllEvents().empty());
    EXPECT_TRUE(rb.events.getEventsByDate("2026-11-02").empty());
    EXPECT_FALSE(rb.events.getEventById(num(1)));
    EXPECT_THROW(rb.events.updateEvent(num(1), makeEvent(1, "2026-11-03")), repositories::NotFoundError);
    EXPECT_THROW(rb.events.deleteEvent(num(1)), repositories::NotFoundError);
    EXPECT_EQ(ra.events.getAllEvents().size(), 1u);
}

TEST_F(RepoFixture, DependenciesCannotCrossUsers) {
    ra.tasks.createTask(makeTask(1));
    ra.tasks.createTask(makeTask(2));
    rb.tasks.createTask(makeTask(1));
    ra.dependencies.addDependency(num(2), num(1));
    EXPECT_TRUE(rb.dependencies.getAllDependencies().empty());
    EXPECT_THROW(rb.dependencies.getPrerequisites(num(2)), repositories::NotFoundError);   // B has no task 2
    EXPECT_THROW(rb.dependencies.addDependency(num(1), num(2)), repositories::NotFoundError);
    EXPECT_THROW(rb.dependencies.removeDependency(num(2), num(1)), repositories::NotFoundError);
    EXPECT_EQ(ra.dependencies.getAllDependencies().size(), 1u);
    // A task body cannot reference another user's task either.
    EXPECT_THROW(rb.tasks.createTask(makeTask(3, "x", {{"prerequisite_ids", json::array({2})}})), models::ValidationError);
}

TEST_F(RepoFixture, DeletingAUserRemovesOnlyTheirData) {
    ra.tasks.createTask(makeTask(1));
    rb.tasks.createTask(makeTask(1));
    mgr.database().exec("DELETE FROM users WHERE id = " + std::to_string(a));
    EXPECT_TRUE(ra.tasks.getAllTasks().empty());
    EXPECT_EQ(rb.tasks.getAllTasks().size(), 1u);
}

TEST(IsolationApi, UserACannotSeeOrTouchUserBsDataOverHttp) {
    AuthHarness h;
    const TestSession a = signUp(h.app, "a@example.com");
    const TestSession b = signUp(h.app, "b@example.com");
    CallOptions oa; oa.session = &a;
    CallOptions ob; ob.session = &b;

    const Reply created = h.post("/api/v1/tasks", json{{"title", "A secret"}, {"duration_min", 30}}.dump(), oa);
    ASSERT_EQ(created.code, 201) << created.raw;
    const std::string id = created.body["id"].dump();
    ASSERT_EQ(h.post("/api/v1/events", json{{"title", "A meeting"}, {"date", "2026-11-02"}, {"start_min", 540}, {"end_min", 600}}.dump(), oa).code, 201);

    EXPECT_EQ(h.get("/api/v1/tasks", ob).body, json::array());
    EXPECT_EQ(h.get("/api/v1/events", ob).body, json::array());
    EXPECT_EQ(h.get("/api/v1/events?date=2026-11-02", ob).body, json::array());
    EXPECT_EQ(h.get("/api/v1/tasks/" + id, ob).code, 404);
    EXPECT_EQ(call(h.app, HTTPMethod::Put, "/api/v1/tasks/" + id, json{{"title", "mine now"}, {"duration_min", 30}}.dump(), ob).code, 404);
    EXPECT_EQ(call(h.app, HTTPMethod::Delete, "/api/v1/tasks/" + id, "", ob).code, 404);
    EXPECT_EQ(call(h.app, HTTPMethod::Delete, "/api/v1/events/1", "", ob).code, 404);
    EXPECT_EQ(h.get("/api/v1/tasks/" + id + "/dependencies", ob).code, 404);

    // Still intact for A.
    EXPECT_EQ(h.get("/api/v1/tasks/" + id, oa).body["title"], "A secret");
    EXPECT_EQ(h.get("/api/v1/events", oa).body.size(), 1u);
}

TEST(IsolationApi, SchedulingFromTheDatabaseOnlyUsesTheCallersData) {
    AuthHarness h;
    const TestSession a = signUp(h.app, "a@example.com");
    const TestSession b = signUp(h.app, "b@example.com");
    CallOptions oa; oa.session = &a;
    CallOptions ob; ob.session = &b;
    ASSERT_EQ(h.post("/api/v1/tasks", json{{"title", "A only"}, {"duration_min", 60}}.dump(), oa).code, 201);

    json req = schedtest::body(schedtest::cfg());   // same helpers as the scheduling API tests
    req["data_source"] = "database";
    req.erase("flexible_tasks");
    req.erase("fixed_events");
    const Reply rb = h.post("/api/v1/schedule/optimize", req.dump(), ob);
    ASSERT_EQ(rb.code, 200) << rb.raw;
    EXPECT_EQ(rb.body.dump().find("A only"), std::string::npos);
    const Reply ra = h.post("/api/v1/schedule/optimize", req.dump(), oa);
    ASSERT_EQ(ra.code, 200) << ra.raw;
    EXPECT_NE(ra.body.dump().find("A only"), std::string::npos);
}
