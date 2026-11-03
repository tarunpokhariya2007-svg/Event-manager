#pragma once
// Scheduling-specific ranking for the PriorityQueue. This only ORDERS candidates; it
// contains no placement/optimisation logic (that is Phase 4).
//
// Task::priority is 1 (lowest) .. 5 (highest), so a LARGER number ranks first.
// Ranking, first rule that differs wins (earlier in the list = extracted earlier):
//   1. higher priority
//   2. has a deadline before has none; then earlier deadline_date (YYYY-MM-DD compares
//      correctly as text), then earlier deadline_min
//   3. earlier earliest_start_min (absent counts as 0)
//   4. longer duration_min (harder to place, so considered first)
//   5. task id via compareIds (numeric ascending, then strings) -> total, deterministic order
// Duplicate policy: one entry per task id (PriorityQueue::push rejects a second one).
#include <optional>
#include <string>
#include "algorithms/IdOrdering.hpp"
#include "algorithms/PriorityQueue.hpp"
#include "models/Validation.hpp"

namespace chronoplan::algorithms {

struct SchedulingCandidate {
    models::Id task_id;
    int priority = 3;                          // 1..5, 5 = most important
    std::optional<std::string> deadline_date;  // YYYY-MM-DD
    int deadline_min = 1440;                   // only meaningful with deadline_date
    int earliest_start_min = 0;
    int duration_min = 0;
    // Phase 4: deadline as minutes relative to the start of the scheduling horizon (negative =
    // already before the horizon). Set by the scheduling engine; the Phase 3 comparator ignores it.
    std::optional<long long> deadline_abs_min;
};

// <0: a ranks before b, >0: b before a, 0: same task id and same ranking fields.
inline int compareCandidates(const SchedulingCandidate& a, const SchedulingCandidate& b) {
    if (a.priority != b.priority) return a.priority > b.priority ? -1 : 1;
    if (a.deadline_date.has_value() != b.deadline_date.has_value()) return a.deadline_date ? -1 : 1;
    if (a.deadline_date) {
        const int c = a.deadline_date->compare(*b.deadline_date);
        if (c != 0) return c < 0 ? -1 : 1;
        if (a.deadline_min != b.deadline_min) return a.deadline_min < b.deadline_min ? -1 : 1;
    }
    if (a.earliest_start_min != b.earliest_start_min) return a.earliest_start_min < b.earliest_start_min ? -1 : 1;
    if (a.duration_min != b.duration_min) return a.duration_min > b.duration_min ? -1 : 1;
    return compareIds(a.task_id, b.task_id);
}

struct CandidateRanksBefore {
    bool operator()(const SchedulingCandidate& a, const SchedulingCandidate& b) const {
        return compareCandidates(a, b) < 0;
    }
};

struct CandidateKey {
    std::string operator()(const SchedulingCandidate& c) const { return c.task_id.key(); }
};

using SchedulingQueue = PriorityQueue<SchedulingCandidate, CandidateRanksBefore, CandidateKey>;

}  // namespace chronoplan::algorithms
