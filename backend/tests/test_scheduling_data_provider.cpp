#include <gtest/gtest.h>
#include "TestDb.hpp"
#include "services/SchedulingDataProvider.hpp"

using namespace chronoplan;
using namespace chronoplan::algorithms;
using chronoplan::services::SchedulingData;
using chronoplan::services::SchedulingDataOptions;
using chronoplan::services::SchedulingDataProvider;
using testutil::makeEvent;
using testutil::makeTask;
using testutil::num;
using testutil::str;

namespace {
constexpr const char* kDate = "2026-11-02";

struct Fixture : ::testing::Test {
    testutil::TempDb tmp;
    database::DatabaseManager mgr{tmp.config()};
    long long uid = testutil::makeUser(mgr.database());
    repositories::Repositories repos{mgr.database(), uid};
    SchedulingDataProvider provider{repos};

    SchedulingDataOptions opts(std::vector<models::Id> completed = {}) {
        SchedulingDataOptions o;
        o.date = kDate;
        o.completed_task_ids = std::move(completed);
        return o;
    }
    std::vector<long long> queueOrder(SchedulingQueue& q) {
        std::vector<long long> out;
        while (auto c = q.pop()) out.push_back(std::stoll(c->task_id.value));
        return out;
    }
};
}  // namespace

TEST_F(Fixture, EmptyDatabaseYieldsEmptyStructures) {
    SchedulingData d = provider.prepareSchedulingData(opts());
    EXPECT_TRUE(d.tasks.empty());
    EXPECT_TRUE(d.events.empty());
    EXPECT_TRUE(d.graph.empty());
    EXPECT_TRUE(d.intervals.empty());
    EXPECT_TRUE(d.queue.empty());
    EXPECT_FALSE(d.cycle.has_value());
    EXPECT_TRUE(d.warnings.empty());
}

TEST_F(Fixture, LoadsTasksEventsAndDependenciesFromRepositories) {
    repos.tasks.createTask(makeTask(1, "A"));
    repos.tasks.createTask(makeTask(2, "B"));
    repos.tasks.createTask(makeTask(3, "C"));
    repos.dependencies.addDependency(num(2), num(1));   // B requires A
    repos.dependencies.addDependency(num(3), num(2));   // C requires B
    repos.events.createEvent(makeEvent(10, kDate, 540, 600));

    EXPECT_EQ(provider.loadTasks().size(), 3u);
    EXPECT_EQ(provider.loadEvents(kDate).size(), 1u);
    EXPECT_EQ(provider.loadDependencies().size(), 2u);

    SchedulingData d = provider.prepareSchedulingData(opts());
    EXPECT_EQ(d.graph.vertexCount(), 3u);
    EXPECT_EQ(d.graph.edgeCount(), 2u);
    EXPECT_TRUE(d.graph.hasEdge(num(1), num(2)));     // prerequisite -> dependent
    EXPECT_TRUE(d.graph.hasEdge(num(2), num(3)));
    EXPECT_FALSE(d.graph.hasEdge(num(2), num(1)));
    EXPECT_EQ(d.graph.topologicalOrder(), (std::vector<models::Id>{num(1), num(2), num(3)}));
    EXPECT_TRUE(d.warnings.empty());
}

TEST_F(Fixture, IntervalIndexHoldsOnlyTargetDateEvents) {
    repos.events.createEvent(makeEvent(1, kDate, 540, 600));
    repos.events.createEvent(makeEvent(2, kDate, 660, 750));
    repos.events.createEvent(makeEvent(3, "2026-11-03", 540, 600));   // other day: excluded
    SchedulingData d = provider.prepareSchedulingData(opts());
    ASSERT_EQ(d.intervals.size(), 2u);
    const auto all = d.intervals.toVector();
    EXPECT_EQ(all[0].start, 540);
    EXPECT_EQ(all[0].end, 600);
    EXPECT_EQ(all[0].id, num(1));
    EXPECT_EQ(all[0].type, IntervalType::Event);
    EXPECT_EQ(all[1].id, num(2));
    EXPECT_TRUE(d.intervals.findAnyOverlap(570, 580).has_value());
    EXPECT_FALSE(d.intervals.findAnyOverlap(600, 660).has_value());   // half-open: touching only
    EXPECT_EQ(d.intervals.findFreeSlots(480, 840),
              (std::vector<TimeSlot>{{480, 540}, {600, 660}, {750, 840}}));
    EXPECT_TRUE(d.intervals.checkInvariants());
}

TEST_F(Fixture, FlexibleEventsCanBeExcluded) {
    repos.events.createEvent(makeEvent(1, kDate, 540, 600));
    repos.events.createEvent(makeEvent(2, kDate, 700, 760, {{"is_flexible", true}}));
    EXPECT_EQ(provider.prepareSchedulingData(opts()).intervals.size(), 2u);
    SchedulingDataOptions o = opts();
    o.include_flexible_events = false;
    SchedulingData d = provider.prepareSchedulingData(o);
    ASSERT_EQ(d.intervals.size(), 1u);
    EXPECT_EQ(d.intervals.toVector()[0].id, num(1));
}

TEST_F(Fixture, NumericAndStringIdsStayDistinct) {
    repos.tasks.createTask(makeTask(7, "numeric seven"));
    repos.tasks.createTask(makeTask("7", "string seven"));
    repos.events.createEvent(makeEvent(7, kDate, 540, 600));
    repos.events.createEvent(makeEvent("7", kDate, 540, 600));
    repos.dependencies.addDependency(str("7"), num(7));   // string "7" requires numeric 7

    SchedulingData d = provider.prepareSchedulingData(opts());
    EXPECT_EQ(d.graph.vertexCount(), 2u);
    EXPECT_TRUE(d.graph.hasEdge(num(7), str("7")));
    EXPECT_FALSE(d.graph.hasEdge(str("7"), num(7)));
    EXPECT_EQ(d.intervals.size(), 2u);                    // same bounds, different id types
    ASSERT_EQ(d.queue.size(), 1u);                        // only numeric 7 is ready
    EXPECT_TRUE(d.queue.top().task_id.numeric);
}

TEST_F(Fixture, QueueHoldsReadyTasksRankedByPriorityThenDeadline) {
    repos.tasks.createTask(makeTask(1, "low", {{"priority", 1}}));
    repos.tasks.createTask(makeTask(2, "high", {{"priority", 5}}));
    repos.tasks.createTask(makeTask(3, "mid soon", {{"priority", 3}, {"deadline_date", "2026-11-03"}}));
    repos.tasks.createTask(makeTask(4, "mid later", {{"priority", 3}, {"deadline_date", "2026-11-09"}}));
    repos.tasks.createTask(makeTask(5, "blocked", {{"priority", 5}}));
    repos.dependencies.addDependency(num(5), num(1));      // 5 waits for 1

    SchedulingData d = provider.prepareSchedulingData(opts());
    EXPECT_EQ(d.queue.size(), 4u);                         // task 5 is blocked
    EXPECT_FALSE(d.queue.contains(num(5).key()));
    EXPECT_EQ(queueOrder(d.queue), (std::vector<long long>{2, 3, 4, 1}));
}

TEST_F(Fixture, CompletedTasksUnlockDependentsAndLeaveTheQueue) {
    repos.tasks.createTask(makeTask(1, "A", {{"priority", 2}}));
    repos.tasks.createTask(makeTask(2, "B", {{"priority", 4}}));
    repos.dependencies.addDependency(num(2), num(1));
    SchedulingData before = provider.prepareSchedulingData(opts());
    EXPECT_EQ(queueOrder(before.queue), (std::vector<long long>{1}));
    SchedulingData after = provider.prepareSchedulingData(opts({num(1)}));
    EXPECT_EQ(queueOrder(after.queue), (std::vector<long long>{2}));
    // Task fields reach the candidate unchanged.
    const SchedulingCandidate c = SchedulingDataProvider::toCandidate(makeTask(9, "x", {
        {"priority", 4}, {"duration_min", 90}, {"deadline_date", "2026-11-05"},
        {"deadline_min", 600}, {"earliest_start_min", 480}}));
    EXPECT_EQ(c.task_id, num(9));
    EXPECT_EQ(c.priority, 4);
    EXPECT_EQ(c.duration_min, 90);
    ASSERT_TRUE(c.deadline_date.has_value());
    EXPECT_EQ(*c.deadline_date, "2026-11-05");
    EXPECT_EQ(c.deadline_min, 600);
    EXPECT_EQ(c.earliest_start_min, 480);
}

TEST_F(Fixture, TaskPrerequisiteIdsInTaskBodyBecomeGraphEdges) {
    repos.tasks.createTask(makeTask(1, "A"));
    repos.tasks.createTask(makeTask(2, "B", {{"prerequisite_ids", nlohmann::json::array({1})}}));
    SchedulingData d = provider.prepareSchedulingData(opts());
    EXPECT_TRUE(d.graph.hasEdge(num(1), num(2)));
    EXPECT_EQ(d.queue.size(), 1u);
}

TEST_F(Fixture, InvalidDateIsRejected) {
    SchedulingDataOptions o;
    o.date = "not-a-date";
    EXPECT_THROW(provider.prepareSchedulingData(o), models::ValidationError);
    EXPECT_THROW(provider.loadEvents("2026-13-45"), models::ValidationError);
}

TEST_F(Fixture, DeletedTaskDisappearsFromEveryStructure) {
    repos.tasks.createTask(makeTask(1, "A"));
    repos.tasks.createTask(makeTask(2, "B"));
    repos.dependencies.addDependency(num(2), num(1));
    repos.tasks.deleteTask(num(1));                        // cascades the dependency row
    SchedulingData d = provider.prepareSchedulingData(opts());
    EXPECT_EQ(d.graph.vertexCount(), 1u);
    EXPECT_EQ(d.graph.edgeCount(), 0u);
    EXPECT_EQ(d.queue.size(), 1u);
}

// ---- pure builders: invalid / inconsistent input is reported, never silently lost --------

TEST(SchedulingDataBuilders, DependencyOnUnknownTaskProducesWarning) {
    std::vector<models::Task> tasks = {makeTask(1, "A"), makeTask(2, "B")};
    std::vector<repositories::Dependency> deps = {{num(2), num(1)}, {num(2), num(99)}, {num(1), num(1)}};
    std::vector<std::string> warnings;
    DependencyGraph g = SchedulingDataProvider::buildDependencyGraph(tasks, deps, warnings);
    EXPECT_EQ(g.edgeCount(), 1u);
    EXPECT_TRUE(g.hasEdge(num(1), num(2)));
    EXPECT_EQ(warnings.size(), 2u);                        // unknown prerequisite + self edge
}

TEST(SchedulingDataBuilders, CyclicStoredDataIsDetectedNotQueued) {
    std::vector<models::Task> tasks = {makeTask(1, "A"), makeTask(2, "B"), makeTask(3, "C")};
    std::vector<repositories::Dependency> deps = {{num(2), num(1)}, {num(1), num(2)}};   // 1 <-> 2
    std::vector<std::string> warnings;
    DependencyGraph g = SchedulingDataProvider::buildDependencyGraph(tasks, deps, warnings);
    EXPECT_TRUE(g.hasCycle());
    SchedulingQueue q = SchedulingDataProvider::buildPriorityQueue(tasks, g, {}, warnings);
    ASSERT_EQ(q.size(), 1u);                               // only the independent task 3
    EXPECT_EQ(q.top().task_id, num(3));
}

TEST(SchedulingDataBuilders, DuplicateEventIntervalsAreReported) {
    std::vector<models::Event> events = {makeEvent(1, kDate, 540, 600), makeEvent(1, kDate, 540, 600)};
    std::vector<std::string> warnings;
    IntervalTree t = SchedulingDataProvider::buildIntervalIndex(events, true, warnings);
    EXPECT_EQ(t.size(), 1u);
    EXPECT_EQ(warnings.size(), 1u);
}

// ---- Phase 4 fix: a task on a cycle is never queued just because its prerequisites are completed ----

TEST(SchedulingDataBuilders, CycleMemberIsNotQueuedWhenItsPrerequisiteIsMarkedCompleted) {
    std::vector<models::Task> tasks = {makeTask(1, "A"), makeTask(2, "B"), makeTask(3, "C"), makeTask(4, "D")};
    // 1 <-> 2 cycle; 3 depends on 2 (downstream of the cycle); 4 is independent.
    std::vector<repositories::Dependency> deps = {{num(2), num(1)}, {num(1), num(2)}, {num(3), num(2)}};   // {task, prerequisite}
    std::vector<std::string> warnings;
    DependencyGraph g = SchedulingDataProvider::buildDependencyGraph(tasks, deps, warnings);
    ASSERT_TRUE(g.hasCycle());
    // Mark task 1 completed: before the fix task 2 (only prerequisite = 1) became "ready".
    SchedulingQueue q = SchedulingDataProvider::buildPriorityQueue(tasks, g, {num(1)}, warnings);
    ASSERT_EQ(q.size(), 1u);
    EXPECT_EQ(q.top().task_id, num(4));
    // Even with the whole cycle completed, the downstream task 3 stays out.
    SchedulingQueue q2 = SchedulingDataProvider::buildPriorityQueue(tasks, g, {num(1), num(2)}, warnings);
    ASSERT_EQ(q2.size(), 1u);
    EXPECT_EQ(q2.top().task_id, num(4));
}
