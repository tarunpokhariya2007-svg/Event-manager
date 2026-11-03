#pragma once
// AVL-balanced, augmented interval tree over half-open TimeIntervals.
//
// Ordering key : (start, end, type, id)  -- strict total order, so ties are deterministic.
// Augmentation : every node stores maxEnd = max(interval.end) over its subtree.
// Duplicates   : an interval equal in ALL four key fields to a stored one is rejected
//                (insert returns false). Different ids/types with identical bounds coexist.
//
// Complexity (n = stored intervals, k = reported intervals):
//   insert / remove / contains : O(log n) worst case (AVL height <= 1.44 log2(n+2))
//   findAnyOverlap             : O(log n)
//   findAllOverlaps            : O(log n + k) typical, O(n) worst case. The pruned
//                                traversal is NOT guaranteed output-sensitive.
//   findFreeSlots              : findAllOverlaps + O(k log k) sort/sweep
//   computeFreeSlots(vector)   : O(m log m) for m inputs (input need not be sorted)
//   toVector / checkInvariants : O(n)
// Space: O(n). No Crow / SQLite dependency.
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "algorithms/TimeInterval.hpp"

namespace chronoplan::algorithms {

namespace detail { struct IntervalNode; }

// Free time inside [windowStart, windowEnd) not covered by `occupied`, chronological,
// zero-length gaps excluded, gaps shorter than minLength excluded. Occupied intervals may
// be unsorted, overlapping, duplicated, or extend beyond the window.
// Throws models::ValidationError if windowStart >= windowEnd or minLength < 1.
std::vector<TimeSlot> computeFreeSlots(const std::vector<TimeInterval>& occupied,
                                       int windowStart, int windowEnd, int minLength = 1);

class IntervalTree {
public:
    IntervalTree();
    ~IntervalTree();
    IntervalTree(IntervalTree&&) noexcept;
    IntervalTree& operator=(IntervalTree&&) noexcept;
    IntervalTree(const IntervalTree&) = delete;
    IntervalTree& operator=(const IntervalTree&) = delete;

    // Returns false (tree unchanged) if an identical interval is already stored.
    bool insert(TimeInterval interval);
    // Removes the interval equal to `interval` (all four key fields). False if absent.
    bool remove(const TimeInterval& interval);
    bool contains(const TimeInterval& interval) const;

    // Range queries; [start, end) must satisfy start < end (ValidationError otherwise).
    std::optional<TimeInterval> findAnyOverlap(int start, int end) const;
    std::optional<TimeInterval> findAnyOverlap(const TimeInterval& range) const;
    // Results are ordered by the tree key (i.e. by start time).
    std::vector<TimeInterval> findAllOverlaps(int start, int end) const;
    std::vector<TimeInterval> findAllOverlaps(const TimeInterval& range) const;

    std::vector<TimeSlot> findFreeSlots(int windowStart, int windowEnd, int minLength = 1) const;

    std::vector<TimeInterval> toVector() const;   // in-order (sorted by key)
    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    int height() const noexcept;                  // 0 for an empty tree
    std::optional<int> maxEnd() const noexcept;   // root augmentation value
    void clear() noexcept;

    // Verifies BST order, AVL balance, stored heights, maxEnd augmentation and size.
    bool checkInvariants(std::string* reason = nullptr) const;

private:
    std::unique_ptr<detail::IntervalNode> root_;
    size_t size_ = 0;
};

}  // namespace chronoplan::algorithms
