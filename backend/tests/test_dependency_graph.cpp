#include <gtest/gtest.h>
#include <algorithm>
#include <map>
#include <random>
#include "AlgoTestUtil.hpp"
#include "algorithms/DependencyGraph.hpp"

using namespace chronoplan::algorithms;
using chronoplan::models::Id;
using namespace algotest;

namespace {
DependencyGraph chain(int n) {   // 1 -> 2 -> ... -> n
    DependencyGraph g;
    for (int i = 1; i <= n; ++i) g.addVertex(num(i));
    for (int i = 1; i < n; ++i) g.addEdge(num(i), num(i + 1));
    return g;
}
std::vector<Id> ids(std::initializer_list<long long> v) { std::vector<Id> o; for (auto x : v) o.push_back(num(x)); return o; }
void expectPrereqsFirst(const DependencyGraph& g, const std::vector<Id>& order) {
    std::map<std::string, size_t> pos;
    for (size_t i = 0; i < order.size(); ++i) pos[order[i].key()] = i;
    ASSERT_EQ(order.size(), g.vertexCount());
    for (const Id& v : g.vertices())
        for (const Id& p : g.getPrerequisites(v)) EXPECT_LT(pos.at(p.key()), pos.at(v.key()));
}
}  // namespace

TEST(DependencyGraph, EmptyGraph) {
    DependencyGraph g;
    EXPECT_TRUE(g.empty());
    EXPECT_FALSE(g.hasCycle());
    EXPECT_FALSE(g.findCycle().has_value());
    EXPECT_TRUE(g.topologicalSort().success);
    EXPECT_TRUE(g.topologicalOrder().empty());
    EXPECT_TRUE(g.readyTasks({}).empty());
    EXPECT_TRUE(g.vertices().empty());
}

TEST(DependencyGraph, VerticesAddRemove) {
    DependencyGraph g;
    EXPECT_TRUE(g.addVertex(num(1)));
    EXPECT_FALSE(g.addVertex(num(1)));
    EXPECT_THROW(g.addVertex(Id{}), GraphError);
    EXPECT_TRUE(g.hasVertex(num(1)));
    EXPECT_FALSE(g.hasVertex(num(2)));
    EXPECT_TRUE(g.removeVertex(num(1)));
    EXPECT_FALSE(g.removeVertex(num(1)));
    EXPECT_TRUE(g.empty());
}

TEST(DependencyGraph, EdgeDirectionConvention) {
    DependencyGraph g = chain(3);            // 1 -> 2 -> 3 : 1 before 2 before 3
    EXPECT_EQ(g.getPrerequisites(num(2)), ids({1}));
    EXPECT_EQ(g.getDependents(num(2)), ids({3}));
    EXPECT_EQ(g.getAdjacent(num(1)), ids({2}));
    EXPECT_TRUE(g.getPrerequisites(num(1)).empty());
    EXPECT_TRUE(g.hasEdge(num(1), num(2)));
    EXPECT_FALSE(g.hasEdge(num(2), num(1)));
    EXPECT_EQ(g.edgeCount(), 2u);
    EXPECT_THROW(g.getPrerequisites(num(99)), MissingVertexError);
    EXPECT_THROW(g.getDependents(num(99)), MissingVertexError);
}

TEST(DependencyGraph, EdgeRejections) {
    DependencyGraph g = chain(3);
    EXPECT_THROW(g.addEdge(num(1), num(1)), SelfEdgeError);
    EXPECT_THROW(g.addEdge(num(1), num(2)), DuplicateEdgeError);
    EXPECT_THROW(g.addEdge(num(1), num(42)), MissingVertexError);
    EXPECT_THROW(g.addEdge(num(42), num(1)), MissingVertexError);
    EXPECT_EQ(g.edgeCount(), 2u);
    EXPECT_FALSE(g.removeEdge(num(3), num(1)));
    EXPECT_FALSE(g.removeEdge(num(1), num(42)));
    EXPECT_TRUE(g.removeEdge(num(1), num(2)));
    EXPECT_FALSE(g.hasEdge(num(1), num(2)));
    EXPECT_TRUE(g.getPrerequisites(num(2)).empty());
    EXPECT_EQ(g.edgeCount(), 1u);
    EXPECT_NO_THROW(g.addEdge(num(1), num(2)));   // can be re-added after removal
}

TEST(DependencyGraph, RemoveVertexDropsIncidentEdges) {
    DependencyGraph g = chain(4);
    g.addEdge(num(1), num(4));
    EXPECT_EQ(g.edgeCount(), 4u);
    EXPECT_TRUE(g.removeVertex(num(2)));     // removes 1->2 and 2->3
    EXPECT_EQ(g.edgeCount(), 2u);
    EXPECT_TRUE(g.getPrerequisites(num(3)).empty());
    EXPECT_EQ(g.getDependents(num(1)), ids({4}));
    EXPECT_EQ(g.getPrerequisites(num(4)).size(), 2u);
    g.clear();
    EXPECT_TRUE(g.empty());
    EXPECT_EQ(g.edgeCount(), 0u);
}

TEST(DependencyGraph, NumericAndStringIdsAreDistinct) {
    DependencyGraph g;
    g.addVertex(num(7));
    g.addVertex(str("7"));
    EXPECT_EQ(g.vertexCount(), 2u);
    g.addEdge(num(7), str("7"));
    EXPECT_TRUE(g.hasEdge(num(7), str("7")));
    EXPECT_FALSE(g.hasEdge(str("7"), num(7)));
    EXPECT_THROW(g.addEdge(num(7), num(7)), SelfEdgeError);
    EXPECT_EQ(g.getPrerequisites(str("7")), std::vector<Id>{num(7)});
    EXPECT_TRUE(g.getPrerequisites(num(7)).empty());
    EXPECT_EQ(g.topologicalOrder(), (std::vector<Id>{num(7), str("7")}));
}

TEST(DependencyGraph, DirectCycle) {
    DependencyGraph g;
    g.addVertex(num(1)); g.addVertex(num(2));
    g.addEdge(num(1), num(2)); g.addEdge(num(2), num(1));
    EXPECT_TRUE(g.hasCycle());
    const auto c = g.findCycle();
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->size(), 2u);
}

TEST(DependencyGraph, IndirectAndLongChainCycles) {
    DependencyGraph g = chain(5);
    EXPECT_FALSE(g.hasCycle());
    g.addEdge(num(5), num(1));               // closes 1->2->3->4->5->1
    EXPECT_TRUE(g.hasCycle());
    EXPECT_EQ(g.findCycle()->size(), 5u);

    DependencyGraph big = chain(50000);     // iterative DFS: no stack overflow
    EXPECT_FALSE(big.hasCycle());
    big.addEdge(num(50000), num(1));
    EXPECT_TRUE(big.hasCycle());
    EXPECT_EQ(big.findCycle()->size(), 50000u);
}

TEST(DependencyGraph, DisconnectedCycleDetectedAndWitnessIsARealCycle) {
    DependencyGraph g = chain(3);                       // acyclic component
    for (int i = 10; i <= 12; ++i) g.addVertex(num(i)); // separate cyclic component
    g.addEdge(num(10), num(11)); g.addEdge(num(11), num(12)); g.addEdge(num(12), num(10));
    g.addVertex(num(50));                               // isolated
    ASSERT_TRUE(g.hasCycle());
    const auto cyc = *g.findCycle();
    for (size_t i = 0; i < cyc.size(); ++i)
        EXPECT_TRUE(g.hasEdge(cyc[i], cyc[(i + 1) % cyc.size()]));
}

TEST(DependencyGraph, WouldCreateCycle) {
    DependencyGraph g = chain(4);
    EXPECT_TRUE(g.wouldCreateCycle(num(4), num(1)));
    EXPECT_TRUE(g.wouldCreateCycle(num(3), num(1)));
    EXPECT_TRUE(g.wouldCreateCycle(num(2), num(2)));
    EXPECT_FALSE(g.wouldCreateCycle(num(1), num(4)));
    EXPECT_THROW(g.wouldCreateCycle(num(1), num(99)), MissingVertexError);
}

TEST(DependencyGraph, TopologicalSortDiamondAndDeterminism) {
    DependencyGraph g;
    for (int i : {4, 3, 2, 1}) g.addVertex(num(i));
    g.addEdge(num(1), num(2)); g.addEdge(num(1), num(3));
    g.addEdge(num(2), num(4)); g.addEdge(num(3), num(4));
    const auto r = g.topologicalSort();
    ASSERT_TRUE(r.success);
    EXPECT_TRUE(r.unresolved.empty());
    EXPECT_EQ(r.order, ids({1, 2, 3, 4}));   // ties: smallest id first
    expectPrereqsFirst(g, r.order);
    EXPECT_EQ(g.topologicalOrder(), r.order);
}

TEST(DependencyGraph, TopologicalSortIsInsertionOrderIndependent) {
    std::vector<std::pair<int, int>> edges = {{1, 5}, {2, 5}, {5, 7}, {3, 6}, {6, 7}, {4, 8}};
    std::vector<Id> reference;
    std::mt19937 rng(5);
    for (int trial = 0; trial < 20; ++trial) {
        std::vector<int> vs = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        std::shuffle(vs.begin(), vs.end(), rng);
        std::shuffle(edges.begin(), edges.end(), rng);
        DependencyGraph g;
        for (int v : vs) g.addVertex(num(v));
        for (auto [a, b] : edges) g.addEdge(num(a), num(b));
        const auto order = g.topologicalOrder();
        expectPrereqsFirst(g, order);
        if (reference.empty()) reference = order; else EXPECT_EQ(order, reference);
    }
}

TEST(DependencyGraph, TopologicalSortDisconnectedAndIsolated) {
    DependencyGraph g;
    for (int i : {1, 2, 3, 10, 20}) g.addVertex(num(i));
    g.addVertex(str("solo"));
    g.addEdge(num(1), num(2)); g.addEdge(num(10), num(20));
    const auto order = g.topologicalOrder();
    EXPECT_EQ(order.size(), 6u);
    expectPrereqsFirst(g, order);
    EXPECT_EQ(order.back(), str("solo"));    // string ids sort after numeric ones
}

TEST(DependencyGraph, TopologicalSortReportsCycle) {
    DependencyGraph g = chain(3);
    g.addVertex(num(4));
    g.addEdge(num(3), num(2));               // 2 <-> 3 cycle; 1 is fine, 4 is isolated
    const auto r = g.topologicalSort();
    EXPECT_FALSE(r.success);
    EXPECT_EQ(r.order, ids({1, 4}));
    EXPECT_EQ(r.unresolved, ids({2, 3}));
    EXPECT_THROW(g.topologicalOrder(), CycleError);
}

TEST(DependencyGraph, ReadyTasks) {
    DependencyGraph g;
    for (int i = 1; i <= 6; ++i) g.addVertex(num(i));
    g.addEdge(num(1), num(3)); g.addEdge(num(2), num(3));   // 3 needs 1 and 2
    g.addEdge(num(3), num(4));
    g.addEdge(num(1), num(5));
    // 6 isolated
    EXPECT_EQ(g.readyTasks({}), ids({1, 2, 6}));                    // no prerequisites
    EXPECT_EQ(g.readyTasks(ids({1})), ids({2, 5, 6}));              // 3 still needs 2
    EXPECT_EQ(g.readyTasks(ids({1, 2})), ids({3, 5, 6}));           // completed excluded
    EXPECT_EQ(g.readyTasks(ids({1, 2, 3, 5, 6})), ids({4}));
    EXPECT_TRUE(g.readyTasks(ids({1, 2, 3, 4, 5, 6})).empty());
    EXPECT_EQ(g.readyTasks(ids({1, 999})), ids({2, 5, 6}));         // unknown completed id ignored
}

TEST(DependencyGraph, ReadyTasksKeepsIdTypes) {
    DependencyGraph g;
    g.addVertex(num(7)); g.addVertex(str("7")); g.addVertex(str("b"));
    g.addEdge(str("7"), str("b"));
    EXPECT_EQ(g.readyTasks({}), (std::vector<Id>{num(7), str("7")}));
    EXPECT_EQ(g.readyTasks({num(7)}), (std::vector<Id>{str("7")}));       // numeric 7 is not "7"
    EXPECT_EQ(g.readyTasks({str("7")}), (std::vector<Id>{num(7), str("b")}));
}

TEST(DependencyGraph, LargeDagTopologicalOrder) {
    const int n = 20000;
    DependencyGraph g;
    for (int i = 1; i <= n; ++i) g.addVertex(num(i));
    std::mt19937 rng(31);
    for (int i = 2; i <= n; ++i)             // edges only point from lower to higher id => DAG
        for (int k = 0; k < 2; ++k) {
            const int p = 1 + static_cast<int>(rng() % (i - 1));
            if (!g.hasEdge(num(p), num(i))) g.addEdge(num(p), num(i));
        }
    EXPECT_FALSE(g.hasCycle());
    expectPrereqsFirst(g, g.topologicalOrder());
}

// ---- Phase 4: cycle-aware helpers (the Phase 3 "cyclic-component exclusion" fix) -------------

TEST(DependencyGraphCycles, CyclicComponentsFindEveryCycleOnce) {
    DependencyGraph g;
    for (int i = 1; i <= 9; ++i) g.addVertex(num(i));
    g.addEdge(num(1), num(2)); g.addEdge(num(2), num(3)); g.addEdge(num(3), num(1));   // cycle {1,2,3}
    g.addEdge(num(4), num(5)); g.addEdge(num(5), num(4));                               // cycle {4,5}
    g.addEdge(num(3), num(6));                                                          // 6 downstream of a cycle
    g.addEdge(num(6), num(7));                                                          // 7 downstream too
    g.addEdge(num(8), num(9));                                                          // plain chain
    const auto comps = g.cyclicComponents();
    ASSERT_EQ(comps.size(), 2u);
    EXPECT_EQ(comps[0], ids({1, 2, 3}));
    EXPECT_EQ(comps[1], ids({4, 5}));
    EXPECT_EQ(g.reachableFrom(ids({1, 2, 3, 4, 5})), ids({6, 7}));
    EXPECT_EQ(g.cycleAffectedVertices(), ids({1, 2, 3, 4, 5, 6, 7}));
}

TEST(DependencyGraphCycles, AcyclicGraphHasNoCyclicComponents) {
    EXPECT_TRUE(chain(50).cyclicComponents().empty());
    EXPECT_TRUE(chain(50).cycleAffectedVertices().empty());
    EXPECT_TRUE(DependencyGraph{}.cyclicComponents().empty());
}

TEST(DependencyGraphCycles, LongCycleDoesNotOverflowTheStack) {
    DependencyGraph g = chain(60000);
    g.addEdge(num(60000), num(1));
    const auto comps = g.cyclicComponents();
    ASSERT_EQ(comps.size(), 1u);
    EXPECT_EQ(comps[0].size(), 60000u);
}

TEST(DependencyGraphCycles, ReadyTasksExcludingCyclesNeverReturnsCycleMembersEvenWhenPrerequisitesAreCompleted) {
    DependencyGraph g;
    for (int i = 1; i <= 5; ++i) g.addVertex(num(i));
    g.addEdge(num(1), num(2)); g.addEdge(num(2), num(1));     // 1 <-> 2
    g.addEdge(num(2), num(3));                                // 3 downstream
    // 4 is independent, 5 follows 4
    g.addEdge(num(4), num(5));
    // The old readyTasks() is deliberately unchanged: with 1 completed, 2 looks ready.
    EXPECT_EQ(g.readyTasks(ids({1})), ids({2, 4}));
    // The cycle-safe version excludes the cycle member and the downstream task.
    EXPECT_EQ(g.readyTasksExcludingCycles(ids({1})), ids({4}));
    EXPECT_EQ(g.readyTasksExcludingCycles(ids({1, 2})), ids({4}));   // 3 is downstream of a cycle: still blocked
    EXPECT_EQ(g.readyTasksExcludingCycles(ids({4})), ids({5}));
}

TEST(DependencyGraphCycles, ReadyTasksExcludingCyclesEqualsReadyTasksOnADag) {
    DependencyGraph g = chain(6);
    for (const auto& done : {ids({}), ids({1}), ids({1, 2, 3})})
        EXPECT_EQ(g.readyTasksExcludingCycles(done), g.readyTasks(done));
}
