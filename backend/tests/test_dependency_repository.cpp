#include <gtest/gtest.h>
#include "TestDb.hpp"
#include "repositories/Repositories.hpp"
#include "repositories/RepositoryErrors.hpp"

using namespace chronoplan;
using namespace chronoplan::repositories;
using testutil::makeTask;
using testutil::num;
using testutil::str;

namespace {
struct Fixture : ::testing::Test {
    testutil::TempDb tmp;
    database::DatabaseManager mgr{tmp.config()};
    long long uid = testutil::makeUser(mgr.database());
    Repositories repos{mgr.database(), uid};
    void addTasks(std::initializer_list<long long> ids) {
        for (auto i : ids) repos.tasks.createTask(makeTask(static_cast<long long>(i), "t" + std::to_string(i)));
    }
};
}  // namespace

TEST_F(Fixture, AddRetrieveAndRemove) {
    addTasks({1, 2, 3});
    auto& d = repos.dependencies;
    d.addDependency(num(3), num(1));
    d.addDependency(num(3), num(2));
    d.addDependency(num(2), num(1));

    const auto pre = d.getPrerequisites(num(3));
    ASSERT_EQ(pre.size(), 2u);
    EXPECT_EQ(pre[0], num(1));
    EXPECT_EQ(pre[1], num(2));

    const auto dependents = d.getDependents(num(1));
    ASSERT_EQ(dependents.size(), 2u);
    EXPECT_EQ(dependents[0], num(3));
    EXPECT_EQ(dependents[1], num(2));
    EXPECT_TRUE(d.getDependents(num(3)).empty());
    EXPECT_EQ(d.getAllDependencies().size(), 3u);

    d.removeDependency(num(3), num(1));
    EXPECT_EQ(d.getPrerequisites(num(3)).size(), 1u);
    EXPECT_THROW(d.removeDependency(num(3), num(1)), NotFoundError);   // already gone
}

TEST_F(Fixture, RejectsSelfDependency) {
    addTasks({1});
    EXPECT_THROW(repos.dependencies.addDependency(num(1), num(1)), models::ValidationError);
    EXPECT_TRUE(repos.dependencies.getPrerequisites(num(1)).empty());
}

TEST_F(Fixture, RejectsDuplicate) {
    addTasks({1, 2});
    repos.dependencies.addDependency(num(2), num(1));
    EXPECT_THROW(repos.dependencies.addDependency(num(2), num(1)), ConflictError);
    EXPECT_EQ(repos.dependencies.getAllDependencies().size(), 1u);
}

TEST_F(Fixture, RejectsNonexistentTasks) {
    addTasks({1});
    EXPECT_THROW(repos.dependencies.addDependency(num(1), num(99)), NotFoundError);
    EXPECT_THROW(repos.dependencies.addDependency(num(99), num(1)), NotFoundError);
    EXPECT_THROW(repos.dependencies.getPrerequisites(num(99)), NotFoundError);
    EXPECT_THROW(repos.dependencies.getDependents(num(99)), NotFoundError);
    EXPECT_THROW(repos.dependencies.addDependency(str("1"), num(1)), NotFoundError);  // "1" is not 1
}

TEST_F(Fixture, RejectsDirectCycle) {
    addTasks({1, 2});
    repos.dependencies.addDependency(num(2), num(1));          // 1 -> 2
    EXPECT_THROW(repos.dependencies.addDependency(num(1), num(2)), CircularDependencyError);
    EXPECT_TRUE(repos.dependencies.getPrerequisites(num(1)).empty());
}

TEST_F(Fixture, RejectsIndirectCycleInLongChain) {
    addTasks({1, 2, 3, 4, 5, 6});
    auto& d = repos.dependencies;
    for (long long i = 2; i <= 6; ++i) d.addDependency(num(i), num(i - 1));   // 1->2->3->4->5->6
    EXPECT_THROW(d.addDependency(num(1), num(6)), CircularDependencyError);
    EXPECT_THROW(d.addDependency(num(1), num(4)), CircularDependencyError);
    EXPECT_TRUE(d.hasCircularDependency(num(2), num(5)));
    EXPECT_FALSE(d.hasCircularDependency(num(5), num(2)));
    EXPECT_EQ(d.getAllDependencies().size(), 5u);
}

TEST_F(Fixture, AllowsValidChainsAndDiamonds) {
    addTasks({1, 2, 3, 4});
    auto& d = repos.dependencies;
    d.addDependency(num(2), num(1));
    d.addDependency(num(3), num(1));
    d.addDependency(num(4), num(2));
    d.addDependency(num(4), num(3));   // diamond: not a cycle
    EXPECT_EQ(d.getPrerequisites(num(4)).size(), 2u);
    EXPECT_FALSE(d.hasCircularDependency(num(4), num(1)));
}

TEST_F(Fixture, PreservesTypedIds) {
    repos.tasks.createTask(makeTask(7, "number"));
    repos.tasks.createTask(makeTask("7", "string"));
    repos.tasks.createTask(makeTask("a", "a"));
    auto& d = repos.dependencies;
    d.addDependency(str("a"), num(7));     // num7 -> a
    d.addDependency(str("7"), str("a"));   // a -> "7"
    // num7 -> "7" is a harmless shortcut: 7 and "7" are different tasks.
    EXPECT_NO_THROW(d.addDependency(str("7"), num(7)));
    // The reverse closes a real cycle (num7 -> a -> "7" -> num7).
    EXPECT_THROW(d.addDependency(num(7), str("7")), CircularDependencyError);

    const auto pre = d.getPrerequisites(str("a"));
    ASSERT_EQ(pre.size(), 1u);
    EXPECT_TRUE(pre[0].numeric);
    const auto strSeven = repos.tasks.getTaskById(str("7"));
    ASSERT_EQ(strSeven->prerequisite_ids.size(), 2u);
    EXPECT_EQ(strSeven->prerequisite_ids[0], str("a"));
    EXPECT_EQ(strSeven->prerequisite_ids[1], num(7));
    EXPECT_TRUE(repos.tasks.getTaskById(num(7))->prerequisite_ids.empty());
    EXPECT_EQ(d.getDependents(num(7)).size(), 2u);   // "a" and "7"
}

TEST_F(Fixture, DeletingTasksCleansUpEveryRelationship) {
    addTasks({1, 2, 3});
    auto& d = repos.dependencies;
    d.addDependency(num(2), num(1));
    d.addDependency(num(3), num(2));
    repos.tasks.deleteTask(num(2));
    EXPECT_TRUE(d.getAllDependencies().empty());
    EXPECT_TRUE(d.getPrerequisites(num(3)).empty());
    EXPECT_TRUE(d.getDependents(num(1)).empty());
}

TEST_F(Fixture, FailedAddLeavesNoPartialState) {
    addTasks({1, 2});
    repos.dependencies.addDependency(num(2), num(1));
    EXPECT_THROW(repos.dependencies.addDependency(num(1), num(2)), CircularDependencyError);
    EXPECT_THROW(repos.dependencies.addDependency(num(2), num(1)), ConflictError);
    EXPECT_EQ(repos.dependencies.getAllDependencies().size(), 1u);
    EXPECT_EQ(mgr.database().transactionDepth(), 0);   // every transaction was closed
}
