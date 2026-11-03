#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <random>
#include "AlgoTestUtil.hpp"
#include "algorithms/IntervalTree.hpp"

using namespace chronoplan::algorithms;
using chronoplan::models::ValidationError;
using namespace algotest;

namespace {
void expectValid(const IntervalTree& t) {
    std::string why;
    EXPECT_TRUE(t.checkInvariants(&why)) << why;
}
// Reference: brute-force overlap list sorted by tree key.
std::vector<TimeInterval> bruteOverlaps(std::vector<TimeInterval> all, int s, int e) {
    std::vector<TimeInterval> out;
    for (auto& i : all) if (i.overlaps(s, e)) out.push_back(i);
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return compareIntervals(a, b) < 0; });
    return out;
}
}  // namespace

TEST(TimeInterval, HalfOpenOverlapSemantics) {
    EXPECT_FALSE(iv(H(9), H(10)).overlaps(iv(H(10), H(11))));   // touching
    EXPECT_FALSE(iv(H(10), H(11)).overlaps(iv(H(9), H(10))));
    EXPECT_TRUE(iv(H(9), H(11)).overlaps(iv(H(10), H(12))));
    EXPECT_TRUE(iv(H(9), H(12)).overlaps(iv(H(10), H(11))));    // containment
    EXPECT_TRUE(iv(H(9), H(10)).overlaps(iv(H(9), H(10))));     // identical
}

TEST(TimeInterval, RejectsInvalid) {
    EXPECT_THROW(TimeInterval(10, 10, num(1)), ValidationError);   // zero length
    EXPECT_THROW(TimeInterval(11, 10, num(1)), ValidationError);   // reversed
    EXPECT_THROW(TimeInterval(1, 2, chronoplan::models::Id{}), ValidationError);   // empty id
    EXPECT_EQ(iv(60, 90).length(), 30);
}

TEST(IntervalTree, EmptyTree) {
    IntervalTree t;
    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.size(), 0u);
    EXPECT_EQ(t.height(), 0);
    EXPECT_FALSE(t.maxEnd().has_value());
    EXPECT_FALSE(t.findAnyOverlap(0, 10).has_value());
    EXPECT_TRUE(t.findAllOverlaps(0, 10).empty());
    EXPECT_FALSE(t.remove(iv(1, 2)));
    EXPECT_FALSE(t.contains(iv(1, 2)));
    expectValid(t);
    EXPECT_EQ(t.findFreeSlots(0, 100), (std::vector<TimeSlot>{{0, 100}}));
}

TEST(IntervalTree, SingleInsertAndQuery) {
    IntervalTree t;
    EXPECT_TRUE(t.insert(iv(H(9), H(10))));
    EXPECT_EQ(t.size(), 1u);
    EXPECT_EQ(*t.maxEnd(), H(10));
    EXPECT_TRUE(t.findAnyOverlap(H(9, 30), H(9, 45)).has_value());
    EXPECT_FALSE(t.findAnyOverlap(H(10), H(11)).has_value());     // touching end
    EXPECT_FALSE(t.findAnyOverlap(H(8), H(9)).has_value());       // touching start
    EXPECT_TRUE(t.findAnyOverlap(H(8), H(9, 1)).has_value());
    expectValid(t);
}

TEST(IntervalTree, RejectsInvalidInputsAndQueries) {
    IntervalTree t;
    EXPECT_THROW(t.insert(TimeInterval{}), ValidationError);   // placeholder interval is invalid
    t.insert(iv(1, 5));
    EXPECT_THROW(t.findAnyOverlap(5, 5), ValidationError);
    EXPECT_THROW(t.findAllOverlaps(6, 5), ValidationError);
    EXPECT_THROW(t.findFreeSlots(10, 10), ValidationError);
    EXPECT_EQ(t.size(), 1u);
}

TEST(IntervalTree, MultipleOverlapsOrderedByStart) {
    IntervalTree t;
    t.insert(iv(H(14), H(15), 3));
    t.insert(iv(H(9), H(11), 1));
    t.insert(iv(H(10), H(12), 2));
    t.insert(iv(H(16), H(17), 4));
    const auto hit = t.findAllOverlaps(H(10, 30), H(14, 30));
    ASSERT_EQ(hit.size(), 3u);
    EXPECT_EQ(hit[0].id, num(1));
    EXPECT_EQ(hit[1].id, num(2));
    EXPECT_EQ(hit[2].id, num(3));
    EXPECT_TRUE(t.findAllOverlaps(H(12), H(14)).empty());   // gap
    expectValid(t);
}

TEST(IntervalTree, DuplicatePolicy) {
    IntervalTree t;
    EXPECT_TRUE(t.insert(iv(60, 120, 1)));
    EXPECT_FALSE(t.insert(iv(60, 120, 1)));                                 // identical: rejected
    EXPECT_TRUE(t.insert(iv(60, 120, 2)));                                  // same bounds, other id
    EXPECT_TRUE(t.insert(iv(60, 120, 1, IntervalType::ScheduledTask)));     // same id, other type
    EXPECT_EQ(t.size(), 3u);
    EXPECT_EQ(t.findAllOverlaps(70, 80).size(), 3u);
    EXPECT_TRUE(t.remove(iv(60, 120, 2)));
    EXPECT_EQ(t.size(), 2u);
    expectValid(t);
}

TEST(IntervalTree, NumericAndStringIdsAreDistinct) {
    IntervalTree t;
    EXPECT_TRUE(t.insert(TimeInterval(60, 120, num(7))));
    EXPECT_TRUE(t.insert(TimeInterval(60, 120, str("7"))));
    EXPECT_EQ(t.size(), 2u);
    EXPECT_TRUE(t.contains(TimeInterval(60, 120, str("7"))));
    EXPECT_TRUE(t.remove(TimeInterval(60, 120, num(7))));
    EXPECT_TRUE(t.contains(TimeInterval(60, 120, str("7"))));
    EXPECT_FALSE(t.contains(TimeInterval(60, 120, num(7))));
    expectValid(t);
}

TEST(IntervalTree, DeleteLeafInternalAndRoot) {
    IntervalTree t;
    for (int i = 0; i < 7; ++i) t.insert(iv(i * 10, i * 10 + 5, i + 1));   // perfectly balanced shape
    expectValid(t);
    EXPECT_TRUE(t.remove(iv(0, 5, 1)));            // leaf
    expectValid(t);
    EXPECT_TRUE(t.remove(iv(10, 15, 2)));          // node whose child was removed / internal
    expectValid(t);
    EXPECT_TRUE(t.remove(iv(30, 35, 4)));          // root of a 7-node AVL (median)
    expectValid(t);
    EXPECT_EQ(t.size(), 4u);
    EXPECT_FALSE(t.contains(iv(30, 35, 4)));
    for (int i : {2, 4, 5, 6}) EXPECT_TRUE(t.contains(iv(i * 10, i * 10 + 5, i + 1)));
    // delete down to empty
    for (int i : {2, 4, 5, 6}) { EXPECT_TRUE(t.remove(iv(i * 10, i * 10 + 5, i + 1))); expectValid(t); }
    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.height(), 0);
}

TEST(IntervalTree, MaxEndMaintainedOnDelete) {
    IntervalTree t;
    t.insert(iv(0, 10, 1));
    t.insert(iv(5, 500, 2));
    t.insert(iv(20, 30, 3));
    EXPECT_EQ(*t.maxEnd(), 500);
    EXPECT_TRUE(t.remove(iv(5, 500, 2)));
    EXPECT_EQ(*t.maxEnd(), 30);
    expectValid(t);
    EXPECT_FALSE(t.findAnyOverlap(40, 600).has_value());
}

TEST(IntervalTree, StaysBalancedOnSortedInsertion) {
    IntervalTree asc, desc;
    const int n = 4096;
    for (int i = 0; i < n; ++i) { asc.insert(iv(i, i + 2, i + 1)); desc.insert(iv(n - i, n - i + 2, i + 1)); }
    expectValid(asc);
    expectValid(desc);
    const double bound = 1.4405 * std::log2(n + 2.0);
    EXPECT_LE(asc.height(), static_cast<int>(bound) + 1);
    EXPECT_LE(desc.height(), static_cast<int>(bound) + 1);
}

TEST(IntervalTree, RepeatedInsertDeleteKeepsInvariants) {
    IntervalTree t;
    std::mt19937 rng(12345);
    std::vector<TimeInterval> live;
    for (int step = 0; step < 4000; ++step) {
        if (live.empty() || rng() % 3 != 0) {
            const int s = static_cast<int>(rng() % 1000);
            TimeInterval x(s, s + 1 + static_cast<int>(rng() % 60), num(rng() % 50));
            if (t.insert(x)) live.push_back(x);
        } else {
            const size_t k = rng() % live.size();
            EXPECT_TRUE(t.remove(live[k]));
            live.erase(live.begin() + static_cast<long>(k));
        }
        if (step % 97 == 0) expectValid(t);
        ASSERT_EQ(t.size(), live.size());
    }
    expectValid(t);
    for (int q = 0; q < 200; ++q) {
        const int s = static_cast<int>(rng() % 1000), e = s + 1 + static_cast<int>(rng() % 100);
        const auto expected = bruteOverlaps(live, s, e);
        EXPECT_EQ(t.findAllOverlaps(s, e), expected);
        EXPECT_EQ(t.findAnyOverlap(s, e).has_value(), !expected.empty());
    }
}

TEST(IntervalTree, LargeDatasetMatchesBruteForce) {
    IntervalTree t;
    std::mt19937 rng(777);
    std::vector<TimeInterval> all;
    for (int i = 0; i < 20000; ++i) {
        const int s = static_cast<int>(rng() % 100000);
        TimeInterval x(s, s + 1 + static_cast<int>(rng() % 500), num(i + 1));
        ASSERT_TRUE(t.insert(x));
        all.push_back(x);
    }
    expectValid(t);
    EXPECT_EQ(t.size(), 20000u);
    for (int q = 0; q < 50; ++q) {
        const int s = static_cast<int>(rng() % 100000), e = s + 1 + static_cast<int>(rng() % 2000);
        EXPECT_EQ(t.findAllOverlaps(s, e), bruteOverlaps(all, s, e));
    }
    EXPECT_EQ(t.toVector().size(), 20000u);
}

TEST(FreeSlots, SpecExample) {
    IntervalTree t;
    t.insert(iv(H(9), H(10), 1));
    t.insert(iv(H(11), H(12, 30), 2));
    t.insert(iv(H(14), H(15), 3));
    const std::vector<TimeSlot> expected = {{H(10), H(11)}, {H(12, 30), H(14)}, {H(15), H(17)}};
    EXPECT_EQ(t.findFreeSlots(H(9), H(17)), expected);
    EXPECT_EQ(computeFreeSlots(t.toVector(), H(9), H(17)), expected);
}

TEST(FreeSlots, UnsortedOverlappingDuplicatedAndClipped) {
    const std::vector<TimeInterval> occ = {iv(H(14), H(16), 1), iv(H(8), H(10), 2), iv(H(9), H(11), 3),
                                           iv(H(9), H(11), 3), iv(H(15), H(15, 30), 4), iv(H(18), H(19), 5),
                                           iv(H(1), H(2), 6)};
    const std::vector<TimeSlot> expected = {{H(11), H(14)}, {H(16), H(17)}};
    EXPECT_EQ(computeFreeSlots(occ, H(9), H(17)), expected);
}

TEST(FreeSlots, BoundariesAndFullCoverage) {
    // Touching the window edges leaves no zero-length slot.
    EXPECT_EQ(computeFreeSlots({iv(H(9), H(10)), iv(H(16), H(17))}, H(9), H(17)),
              (std::vector<TimeSlot>{{H(10), H(16)}}));
    // Fully booked and over-covered windows.
    EXPECT_TRUE(computeFreeSlots({iv(H(8), H(18))}, H(9), H(17)).empty());
    // Back-to-back intervals produce no zero-length gap.
    EXPECT_EQ(computeFreeSlots({iv(H(9), H(10), 1), iv(H(10), H(11), 2)}, H(9), H(12)),
              (std::vector<TimeSlot>{{H(11), H(12)}}));
    // Intervals only touching the window from outside are ignored.
    EXPECT_EQ(computeFreeSlots({iv(H(8), H(9)), iv(H(17), H(18))}, H(9), H(17)),
              (std::vector<TimeSlot>{{H(9), H(17)}}));
}

TEST(FreeSlots, MinLengthAndValidation) {
    EXPECT_EQ(computeFreeSlots({iv(H(9), H(10)), iv(H(10, 20), H(12))}, H(9), H(13), 60),
              (std::vector<TimeSlot>{{H(12), H(13)}}));   // the 20-minute gap is dropped
    EXPECT_THROW(computeFreeSlots({}, 10, 10), ValidationError);
    EXPECT_THROW(computeFreeSlots({}, 10, 20, 0), ValidationError);
}
