#pragma once
// Placement of ONE task into free time: SlotScanner (free-slot enumeration over the Phase 3
// IntervalTree) and TaskPlacer (the earliest-fit placement algorithm, including task splitting).
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "algorithms/IntervalTree.hpp"
#include "scheduler/SchedulingContext.hpp"

namespace chronoplan::scheduler {

// Why (part of) a task could not be placed. Codes are stable API strings (see reasonCode()).
enum class UnscheduledReason {
    None,
    InvalidConstraints,            // invalid_constraints
    OutsideHorizon,                // outside_horizon
    DeadlineExceeded,              // deadline_exceeded
    InsufficientTimeBeforeDeadline,// insufficient_time_before_deadline
    NoAvailableSlot,               // no_available_slot
    NonSplittableNoSlot,           // non_splittable_no_slot
    MinChunkUnsatisfiable,         // min_chunk_unsatisfiable
    ExceedsDailyWorkload,          // exceeds_daily_workload
    NoFeasibleWindow,              // no_feasible_window
    BlockedByPrerequisite,         // blocked_by_prerequisite
    PrerequisiteCancelled,         // prerequisite_cancelled
    UnknownPrerequisite,           // unknown_prerequisite
    CircularDependency,            // circular_dependency
    BlockedByCycle                 // blocked_by_cycle
};
const char* reasonCode(UnscheduledReason r);
const char* suggestedAction(UnscheduledReason r);

struct PlacedSession {
    int start_abs = 0;     // absolute minutes (see CalendarDate.hpp)
    int end_abs = 0;
    int day_index = 0;
    int start_min() const { return start_abs - day_index * kMinutesPerDay; }
    int end_min() const { return end_abs - day_index * kMinutesPerDay; }
};

enum class PlacementOutcome { Complete, Partial, None };

struct TaskPlacement {
    size_t task_index = 0;
    PlacementOutcome outcome = PlacementOutcome::None;
    std::vector<PlacedSession> sessions;          // chronological, non-overlapping
    int scheduled_min = 0;
    int remaining_min = 0;
    int rank = 0;                                 // order in which the engine placed the task (1-based)
    long long ready_abs = 0;                      // earliest start imposed by prerequisites
    UnscheduledReason reason = UnscheduledReason::None;   // set when remaining_min > 0
    std::string reason_text;
    std::optional<int> completion_abs;            // end of the final session when Complete
};

// Enumerates free slots (inside availability windows on active weekdays) of an IntervalTree.
class SlotScanner {
public:
    struct Region {
        long long lo = 0;                // absolute [lo, hi) range to look at
        long long hi = 0;
        int tod_start = 0;               // per-day time-of-day limits (earliest_start / latest_completion)
        int tod_end = kMinutesPerDay;
    };
    // `padding`: free time is also kept `padding` minutes away from every ScheduledTask interval
    // (config.min_break_min). Events are never padded. Padding 0 = plain free time.
    SlotScanner(const SchedulingConstraints& c, const algorithms::IntervalTree& occupied, int padding)
        : c_(c), tree_(occupied), padding_(padding) {}

    // Visits free slots chronologically; the visitor returns false to stop early.
    // Arguments: day index, slot start (absolute), slot end (absolute).
    void forEachFreeSlot(const Region& r, const std::function<bool(int, int, int)>& visit) const;
    // Sum of slot lengths in the region (no workload cap, no chunk rules).
    long long freeMinutes(const Region& r) const;

private:
    const SchedulingConstraints& c_;
    const algorithms::IntervalTree& tree_;
    int padding_;
};

class TaskPlacer {
public:
    // `occupied` holds the fixed events and every session placed so far; `day_load` the task
    // minutes already placed per day. Both are updated by place().
    TaskPlacer(const SchedulingContext& ctx, algorithms::IntervalTree& occupied, std::vector<int>& day_load)
        : ctx_(ctx), tree_(occupied), load_(day_load) {}

    // Earliest-fit placement of task `index`, starting no earlier than `ready_abs`.
    // Sessions are committed (inserted into the tree) even for a Partial outcome.
    TaskPlacement place(size_t index, long long ready_abs);

private:
    UnscheduledReason diagnose(size_t index, const TaskPlacement& p, long long lo, long long hi,
                               std::string& text) const;
    const SchedulingContext& ctx_;
    algorithms::IntervalTree& tree_;
    std::vector<int>& load_;
};

}  // namespace chronoplan::scheduler
