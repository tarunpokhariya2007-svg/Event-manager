#include <gtest/gtest.h>
#include <algorithm>
#include <map>
#include <random>
#include <set>
#include "AlgoTestUtil.hpp"
#include "algorithms/PriorityQueue.hpp"
#include "algorithms/SchedulingPriority.hpp"

using namespace chronoplan::algorithms;
using namespace algotest;

namespace {
struct Item { int id; int prio; };
struct ItemBefore {   // lower prio value first; ties broken by id for determinism
    bool operator()(const Item& a, const Item& b) const {
        return a.prio != b.prio ? a.prio < b.prio : a.id < b.id;
    }
};
struct ItemKey { std::string operator()(const Item& i) const { return std::to_string(i.id); } };
using Q = PriorityQueue<Item, ItemBefore, ItemKey>;

SchedulingCandidate cand(long long id, int prio, std::optional<std::string> dl = std::nullopt,
                         int dlMin = 1440, int earliest = 0, int dur = 60) {
    SchedulingCandidate c;
    c.task_id = num(id); c.priority = prio; c.deadline_date = dl; c.deadline_min = dlMin;
    c.earliest_start_min = earliest; c.duration_min = dur;
    return c;
}
}  // namespace

TEST(PriorityQueue, EmptyQueueIsSafe) {
    Q q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
    EXPECT_EQ(q.peek(), nullptr);
    EXPECT_THROW(q.top(), std::out_of_range);
    EXPECT_FALSE(q.pop().has_value());
    EXPECT_FALSE(q.remove("1"));
    EXPECT_FALSE(q.update({1, 1}));
    EXPECT_TRUE(q.isValidHeap());
}

TEST(PriorityQueue, SingleAndOrdering) {
    Q q;
    EXPECT_TRUE(q.push({1, 5}));
    EXPECT_EQ(q.top().id, 1);
    for (auto [id, p] : std::vector<std::pair<int, int>>{{2, 3}, {3, 9}, {4, 1}, {5, 7}}) {
        q.push({id, p});
        EXPECT_TRUE(q.isValidHeap());
    }
    EXPECT_EQ(q.size(), 5u);
    EXPECT_EQ(q.peek()->id, 4);                  // smallest prio first (min-heap comparator)
    std::vector<int> order;
    while (auto x = q.pop()) { order.push_back(x->id); EXPECT_TRUE(q.isValidHeap()); }
    EXPECT_EQ(order, (std::vector<int>{4, 2, 1, 5, 3}));
    EXPECT_TRUE(q.empty());
}

TEST(PriorityQueue, EqualPrioritiesAreDeterministic) {
    Q q;
    for (int id : {5, 3, 9, 1, 7}) q.push({id, 2});
    std::vector<int> order;
    while (auto x = q.pop()) order.push_back(x->id);
    EXPECT_EQ(order, (std::vector<int>{1, 3, 5, 7, 9}));   // tie-break by id
}

TEST(PriorityQueue, DuplicatePolicyRejectsSecondEntry) {
    Q q;
    EXPECT_TRUE(q.push({1, 5}));
    EXPECT_FALSE(q.push({1, 1}));     // same key, different priority: rejected, unchanged
    EXPECT_EQ(q.size(), 1u);
    EXPECT_EQ(q.top().prio, 5);
    EXPECT_THROW(q.assign({{1, 1}, {1, 2}}), std::invalid_argument);
    EXPECT_EQ(q.size(), 1u);          // unchanged after the failed assign
}

TEST(PriorityQueue, UpdateAndRemoveByKey) {
    Q q;
    for (int i = 1; i <= 10; ++i) q.push({i, i * 10});
    EXPECT_TRUE(q.update({10, 1}));          // heapify-up: becomes the top
    EXPECT_EQ(q.top().id, 10);
    EXPECT_TRUE(q.update({10, 500}));        // heapify-down: becomes the last
    EXPECT_EQ(q.top().id, 1);
    EXPECT_TRUE(q.isValidHeap());
    EXPECT_TRUE(q.remove("5"));
    EXPECT_FALSE(q.contains("5"));
    EXPECT_FALSE(q.remove("5"));
    EXPECT_TRUE(q.remove("1"));              // remove the top
    EXPECT_EQ(q.top().id, 2);
    EXPECT_EQ(q.size(), 8u);
    ASSERT_NE(q.find("10"), nullptr);
    EXPECT_EQ(q.find("10")->prio, 500);
    EXPECT_TRUE(q.isValidHeap());
    q.clear();
    EXPECT_TRUE(q.empty());
    EXPECT_TRUE(q.isValidHeap());
}

TEST(PriorityQueue, AssignBuildsValidHeap) {
    std::vector<Item> items;
    for (int i = 0; i < 1000; ++i) items.push_back({i, (i * 7919) % 101});
    Q q;
    q.assign(items);
    EXPECT_TRUE(q.isValidHeap());
    std::sort(items.begin(), items.end(), ItemBefore{});
    for (const Item& expected : items) { auto x = q.pop(); ASSERT_TRUE(x.has_value()); EXPECT_EQ(x->id, expected.id); }
}

TEST(PriorityQueue, RandomisedAgainstReferenceModel) {
    std::mt19937 rng(2024);
    Q q;
    std::map<int, int> model;                 // id -> prio reference
    auto refTop = [&]() {
        Item best{-1, 0}; bool any = false;
        for (auto [id, p] : model) if (!any || ItemBefore{}({id, p}, best)) { best = {id, p}; any = true; }
        return best;
    };
    for (int step = 0; step < 20000; ++step) {
        const int id = static_cast<int>(rng() % 300), p = static_cast<int>(rng() % 40);
        switch (rng() % 5) {
            case 0: case 1: EXPECT_EQ(q.push({id, p}), model.emplace(id, p).second); break;
            case 2: { const bool had = model.count(id) > 0; EXPECT_EQ(q.update({id, p}), had); if (had) model[id] = p; break; }
            case 3: { const bool had = model.erase(id) > 0; EXPECT_EQ(q.remove(std::to_string(id)), had); break; }
            default: {
                auto x = q.pop();
                if (model.empty()) { EXPECT_FALSE(x.has_value()); }
                else { const Item e = refTop(); ASSERT_TRUE(x.has_value()); EXPECT_EQ(x->id, e.id); EXPECT_EQ(x->prio, e.prio); model.erase(e.id); }
            }
        }
        ASSERT_EQ(q.size(), model.size());
        if (step % 50 == 0) { ASSERT_TRUE(q.isValidHeap()); }
    }
    EXPECT_TRUE(q.isValidHeap());
}

TEST(PriorityQueue, LargeDatasetExtractsSorted) {
    std::mt19937 rng(99);
    Q q;
    std::vector<Item> items;
    for (int i = 0; i < 100000; ++i) { Item x{i, static_cast<int>(rng() % 1000)}; items.push_back(x); q.push(x); }
    std::sort(items.begin(), items.end(), ItemBefore{});
    for (const Item& e : items) { auto x = q.pop(); ASSERT_TRUE(x.has_value()); ASSERT_EQ(x->id, e.id); }
    EXPECT_TRUE(q.empty());
}

TEST(PriorityQueue, MaxHeapViaComparator) {
    PriorityQueue<int, std::greater<int>, std::function<std::string(const int&)>> q(
        std::greater<int>(), [](const int& v) { return std::to_string(v); });
    for (int v : {3, 8, 1, 6}) q.push(v);
    EXPECT_EQ(q.top(), 8);
    EXPECT_TRUE(q.isValidHeap());
}

// ---- scheduling comparator ------------------------------------------------------------

TEST(SchedulingPriority, HigherPriorityValueRanksFirst) {   // Task priority 5 = most important
    EXPECT_LT(compareCandidates(cand(1, 5), cand(2, 4)), 0);
    EXPECT_GT(compareCandidates(cand(1, 1), cand(2, 2)), 0);
}

TEST(SchedulingPriority, DeadlineRules) {
    EXPECT_LT(compareCandidates(cand(1, 3, "2026-11-02"), cand(2, 3, "2026-11-03")), 0);
    EXPECT_LT(compareCandidates(cand(1, 3, "2026-11-02", 600), cand(2, 3, "2026-11-02", 900)), 0);
    EXPECT_LT(compareCandidates(cand(1, 3, "2026-12-31"), cand(2, 3)), 0);          // any deadline beats none
    EXPECT_LT(compareCandidates(cand(9, 4), cand(1, 3, "2026-11-02")), 0);          // priority outranks deadline
}

TEST(SchedulingPriority, EarliestStartDurationThenIdTieBreaks) {
    EXPECT_LT(compareCandidates(cand(2, 3, std::nullopt, 1440, 60), cand(1, 3, std::nullopt, 1440, 120)), 0);
    EXPECT_LT(compareCandidates(cand(2, 3, std::nullopt, 1440, 0, 120), cand(1, 3, std::nullopt, 1440, 0, 60)), 0);
    EXPECT_LT(compareCandidates(cand(2, 3), cand(10, 3)), 0);                       // 2 < 10 numerically
    SchedulingCandidate s = cand(1, 3); s.task_id = str("a");
    EXPECT_LT(compareCandidates(cand(99, 3), s), 0);                                // numeric before string
    EXPECT_EQ(compareCandidates(cand(1, 3), cand(1, 3)), 0);
}

TEST(SchedulingPriority, QueueExtractionOrder) {
    SchedulingQueue q;
    q.push(cand(1, 3, "2026-11-05"));
    q.push(cand(2, 5));
    q.push(cand(3, 3, "2026-11-02"));
    q.push(cand(4, 3));
    q.push(cand(5, 5, "2026-11-01"));
    EXPECT_FALSE(q.push(cand(5, 1)));   // one entry per task
    std::vector<long long> order;
    while (auto c = q.pop()) order.push_back(std::stoll(c->task_id.value));
    EXPECT_EQ(order, (std::vector<long long>{5, 2, 3, 1, 4}));
}

TEST(SchedulingPriority, NumericAndStringIdsCoexistInQueue) {
    SchedulingQueue q;
    SchedulingCandidate s = cand(7, 3); s.task_id = str("7");
    EXPECT_TRUE(q.push(cand(7, 3)));
    EXPECT_TRUE(q.push(s));
    EXPECT_EQ(q.size(), 2u);
    EXPECT_TRUE(q.pop()->task_id.numeric);   // numeric 7 first, then "7"
    EXPECT_FALSE(q.pop()->task_id.numeric);
}
