#include "scheduler/SchedulingObjective.hpp"
#include <algorithm>
#include "algorithms/IdOrdering.hpp"
#include "scheduler/CalendarDate.hpp"

namespace chronoplan::scheduler {

using algorithms::compareCandidates;
using algorithms::compareIds;
using algorithms::SchedulingCandidate;

const char* toString(Objective o) {
    switch (o) {
        case Objective::Balanced: return "balanced";
        case Objective::Priority: return "priority";
        case Objective::Deadline: return "deadline";
        case Objective::Compact: return "compact";
        case Objective::Edf: return "edf";
    }
    return "balanced";
}

Objective parseObjective(const std::string& n) {
    if (n == "balanced") return Objective::Balanced;
    if (n == "priority") return Objective::Priority;
    if (n == "deadline") return Objective::Deadline;
    if (n == "compact") return Objective::Compact;
    if (n == "edf") return Objective::Edf;
    throw models::ValidationError("unknown scheduling objective '" + n + "'");
}

int urgencyPoints(const SchedulingCandidate& c) {
    if (!c.deadline_abs_min) return 0;
    const long long days = (std::max)(0LL, floorDiv(*c.deadline_abs_min, kMinutesPerDay));
    return static_cast<int>((std::max)(0LL, 300 - 30 * days));
}

int balancedScore(const SchedulingCandidate& c) { return c.priority * 100 + urgencyPoints(c); }

namespace {
int cmp(long long a, long long b) { return a < b ? -1 : (a > b ? 1 : 0); }

// Candidates with a deadline before candidates without one.
int hasDeadlineFirst(const SchedulingCandidate& a, const SchedulingCandidate& b) {
    if (a.deadline_abs_min.has_value() == b.deadline_abs_min.has_value()) return 0;
    return a.deadline_abs_min ? -1 : 1;
}
}  // namespace

int compareForObjective(Objective o, const SchedulingCandidate& a, const SchedulingCandidate& b) {
    switch (o) {
        case Objective::Priority:
            return compareCandidates(a, b);
        case Objective::Balanced: {
            const int sa = balancedScore(a), sb = balancedScore(b);
            if (sa != sb) return sa > sb ? -1 : 1;
            return compareCandidates(a, b);
        }
        case Objective::Compact: {
            if (a.duration_min != b.duration_min) return a.duration_min < b.duration_min ? -1 : 1;
            return compareCandidates(a, b);
        }
        case Objective::Deadline: {
            if (const int h = hasDeadlineFirst(a, b)) return h;
            if (a.deadline_abs_min) {
                const long long da = floorDiv(*a.deadline_abs_min, kMinutesPerDay);
                const long long db = floorDiv(*b.deadline_abs_min, kMinutesPerDay);
                if (da != db) return cmp(da, db);
                if (a.priority != b.priority) return a.priority > b.priority ? -1 : 1;
                if (*a.deadline_abs_min != *b.deadline_abs_min)
                    return cmp(*a.deadline_abs_min, *b.deadline_abs_min);
            } else if (a.priority != b.priority) {
                return a.priority > b.priority ? -1 : 1;
            }
            if (a.earliest_start_min != b.earliest_start_min)
                return a.earliest_start_min < b.earliest_start_min ? -1 : 1;
            if (a.duration_min != b.duration_min) return a.duration_min > b.duration_min ? -1 : 1;
            return compareIds(a.task_id, b.task_id);
        }
        case Objective::Edf: {
            if (const int h = hasDeadlineFirst(a, b)) return h;
            if (a.deadline_abs_min && *a.deadline_abs_min != *b.deadline_abs_min)
                return cmp(*a.deadline_abs_min, *b.deadline_abs_min);
            return compareIds(a.task_id, b.task_id);
        }
    }
    return compareCandidates(a, b);
}

}  // namespace chronoplan::scheduler
