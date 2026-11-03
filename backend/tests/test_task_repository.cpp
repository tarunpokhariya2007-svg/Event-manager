#include <gtest/gtest.h>
#include "TestDb.hpp"
#include "repositories/RepositoryErrors.hpp"
#include "repositories/TaskRepository.hpp"

using namespace chronoplan;
using namespace chronoplan::repositories;
using nlohmann::json;
using testutil::makeTask;
using testutil::num;
using testutil::str;

namespace {
struct Fixture : ::testing::Test {
    testutil::TempDb tmp;
    database::DatabaseManager mgr{tmp.config()};
    long long uid = testutil::makeUser(mgr.database());
    TaskRepository repo{mgr.database(), uid};
};
}  // namespace

TEST_F(Fixture, CreateAndRetrieveRoundTripsEveryField) {
    models::Task t = makeTask(5, "Write report", {{"description", "desc"}, {"technical_context", "ctx"},
        {"priority", 5}, {"duration_min", 120}, {"min_chunk_min", 20}, {"allow_split", false},
        {"deadline_date", "2026-12-24"}, {"deadline_min", 900}, {"earliest_start_min", 60},
        {"latest_completion_min", 1000}, {"status", "in progress"}});
    repo.createTask(t);
    const auto got = repo.getTaskById(num(5));
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got->toJson(), t.toJson());
}

TEST_F(Fixture, OptionalFieldsStayNull) {
    repo.createTask(makeTask(1));
    const auto got = repo.getTaskById(num(1));
    ASSERT_TRUE(got);
    EXPECT_FALSE(got->description || got->technical_context || got->deadline_date ||
                 got->earliest_start_min || got->latest_completion_min || got->status);
    const json j = got->toJson();
    EXPECT_TRUE(j["description"].is_null());
    EXPECT_TRUE(j["earliest_start_min"].is_null());
    EXPECT_EQ(j["deadline_min"], 1440);
}

TEST_F(Fixture, GetAllReturnsInsertionOrder) {
    repo.createTask(makeTask(3, "c"));
    repo.createTask(makeTask("a", "a"));
    repo.createTask(makeTask(1, "b"));
    const auto all = repo.getAllTasks();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].title, "c");
    EXPECT_EQ(all[1].title, "a");
    EXPECT_EQ(all[2].title, "b");
    EXPECT_TRUE(repo.getAllTasks().size() == 3);
}

TEST_F(Fixture, MissingTask) {
    EXPECT_FALSE(repo.getTaskById(num(99)).has_value());
    EXPECT_FALSE(repo.taskExists(num(99)));
    EXPECT_THROW(repo.updateTask(num(99), makeTask(99)), NotFoundError);
    EXPECT_THROW(repo.deleteTask(num(99)), NotFoundError);
}

TEST_F(Fixture, DuplicateSameTypeIdRejected) {
    repo.createTask(makeTask(7));
    EXPECT_THROW(repo.createTask(makeTask(7, "other")), ConflictError);
    repo.createTask(makeTask("x"));
    EXPECT_THROW(repo.createTask(makeTask("x")), ConflictError);
    EXPECT_EQ(repo.getAllTasks().size(), 2u);   // failed creates left nothing behind
}

TEST_F(Fixture, NumericSevenAndStringSevenCoexist) {
    repo.createTask(makeTask(7, "number seven"));
    repo.createTask(makeTask("7", "string seven"));
    EXPECT_EQ(repo.getTaskById(num(7))->title, "number seven");
    EXPECT_EQ(repo.getTaskById(str("7"))->title, "string seven");
    EXPECT_TRUE(repo.getTaskById(num(7))->toJson()["id"].is_number_integer());
    EXPECT_TRUE(repo.getTaskById(str("7"))->toJson()["id"].is_string());

    repo.deleteTask(num(7));                          // must not touch the string twin
    EXPECT_FALSE(repo.taskExists(num(7)));
    EXPECT_TRUE(repo.taskExists(str("7")));
}

TEST_F(Fixture, IdIsAssignedWhenOmitted) {
    const auto a = repo.createTask(makeTask(nullptr, "a"));
    const auto b = repo.createTask(makeTask(nullptr, "b"));
    EXPECT_TRUE(a.id.numeric);
    EXPECT_EQ(a.id.value, "1");
    EXPECT_EQ(b.id.value, "2");
    repo.createTask(makeTask("9", "string ids do not affect numeric assignment"));
    EXPECT_EQ(repo.createTask(makeTask(nullptr, "c")).id.value, "3");
}

TEST_F(Fixture, UpdateIsFullReplacement) {
    repo.createTask(makeTask(1, "old", {{"description", "keep?"}, {"status", "todo"}, {"earliest_start_min", 60}}));
    const auto updated = repo.updateTask(num(1), makeTask(nullptr, "new", {{"duration_min", 90}}));
    EXPECT_EQ(updated.id, num(1));
    const auto got = repo.getTaskById(num(1));
    EXPECT_EQ(got->title, "new");
    EXPECT_EQ(got->duration_min, 90);
    EXPECT_FALSE(got->description.has_value());       // absent in the replacement -> NULL
    EXPECT_FALSE(got->status.has_value());
    EXPECT_FALSE(got->earliest_start_min.has_value());
}

TEST_F(Fixture, InvalidDataIsRejectedBeforeWriting) {
    models::Task bad = makeTask(1);
    bad.priority = 9;
    EXPECT_THROW(repo.createTask(bad), models::ValidationError);
    EXPECT_FALSE(repo.taskExists(num(1)));
    repo.createTask(makeTask(1));
    EXPECT_THROW(repo.updateTask(num(1), bad), models::ValidationError);
    EXPECT_EQ(repo.getTaskById(num(1))->priority, 3);
}

TEST_F(Fixture, PrerequisitesAreStoredRelationallyAndReplaced) {
    repo.createTask(makeTask(1));
    repo.createTask(makeTask("b"));
    repo.createTask(makeTask(3, "c", {{"prerequisite_ids", json::array({1, "b"})}}));
    auto got = repo.getTaskById(num(3));
    ASSERT_EQ(got->prerequisite_ids.size(), 2u);
    EXPECT_EQ(got->prerequisite_ids[0], num(1));
    EXPECT_EQ(got->prerequisite_ids[1], str("b"));
    EXPECT_EQ(repo.getAllTasks()[2].prerequisite_ids.size(), 2u);

    repo.updateTask(num(3), makeTask(nullptr, "c", {{"prerequisite_ids", json::array({"b"})}}));
    got = repo.getTaskById(num(3));
    ASSERT_EQ(got->prerequisite_ids.size(), 1u);
    EXPECT_EQ(got->prerequisite_ids[0], str("b"));
}

TEST_F(Fixture, UnknownPrerequisiteRollsBackTheWholeCreate) {
    EXPECT_THROW(repo.createTask(makeTask(2, "x", {{"prerequisite_ids", json::array({42})}})),
                 models::ValidationError);
    EXPECT_FALSE(repo.taskExists(num(2)));
}

TEST_F(Fixture, CyclicPrerequisiteUpdateRollsBackTaskChanges) {
    repo.createTask(makeTask(1, "a"));
    repo.createTask(makeTask(2, "b", {{"prerequisite_ids", json::array({1})}}));
    // making 1 depend on 2 closes a cycle; the title change must not survive
    EXPECT_THROW(repo.updateTask(num(1), makeTask(nullptr, "renamed", {{"prerequisite_ids", json::array({2})}})),
                 CircularDependencyError);
    EXPECT_EQ(repo.getTaskById(num(1))->title, "a");
}

TEST_F(Fixture, DeleteRemovesDependencyRowsBothDirections) {
    repo.createTask(makeTask(1));
    repo.createTask(makeTask(2, "b", {{"prerequisite_ids", json::array({1})}}));
    repo.createTask(makeTask(3, "c", {{"prerequisite_ids", json::array({2})}}));
    repo.deleteTask(num(2));
    EXPECT_TRUE(repo.getTaskById(num(3))->prerequisite_ids.empty());
    DependencyRepository deps(mgr.database(), uid);
    EXPECT_TRUE(deps.getAllDependencies().empty());
}

TEST_F(Fixture, PersistsAfterReopening) {
    repo.createTask(makeTask(1, "persisted", {{"description", "d"}, {"deadline_date", "2027-01-31"}}));
    repo.createTask(makeTask("1", "string twin", {{"prerequisite_ids", json::array({1})}}));
    TaskRepository other(mgr.database(), uid);   // same file, fresh repository
    EXPECT_EQ(other.getAllTasks().size(), 2u);

    // genuinely close and reopen the database file
    database::DatabaseManager second(tmp.config());   // WAL allows a second connection
    TaskRepository reopened(second.database(), uid);
    const auto got = reopened.getTaskById(str("1"));
    ASSERT_TRUE(got);
    EXPECT_EQ(got->title, "string twin");
    ASSERT_EQ(got->prerequisite_ids.size(), 1u);
    EXPECT_EQ(got->prerequisite_ids[0], num(1));
    EXPECT_EQ(reopened.getTaskById(num(1))->deadline_date, "2027-01-31");
}

TEST(TaskRepositoryReopen, DataSurvivesClosingTheDatabase) {
    testutil::TempDb tmp;
    {
        database::DatabaseManager m(tmp.config());
        TaskRepository r(m.database(), testutil::makeUser(m.database()));
        r.createTask(makeTask(1, "survivor"));
    }
    database::DatabaseManager m(tmp.config());
    TaskRepository r(m.database(), 1);   // the first inserted user has id 1
    ASSERT_TRUE(r.getTaskById(num(1)));
    EXPECT_EQ(r.getTaskById(num(1))->title, "survivor");
}
