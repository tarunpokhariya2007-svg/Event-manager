#include "scheduler/TaskPlacement.hpp"
#include <algorithm>

namespace chronoplan::scheduler {

using algorithms::IntervalType;
using algorithms::TimeInterval;
using algorithms::TimeSlot;

const char* reasonCode(UnscheduledReason r) {
    switch (r) {
        case UnscheduledReason::None: return "none";
        case UnscheduledReason::InvalidConstraints: return "invalid_constraints";
        case UnscheduledReason::OutsideHorizon: return "outside_horizon";
        case UnscheduledReason::DeadlineExceeded: return "deadline_exceeded";
        case UnscheduledReason::InsufficientTimeBeforeDeadline: return "insufficient_time_before_deadline";
        case UnscheduledReason::NoAvailableSlot: return "no_available_slot";
        case UnscheduledReason::NonSplittableNoSlot: return "non_splittable_no_slot";
        case UnscheduledReason::MinChunkUnsatisfiable: return "min_chunk_unsatisfiable";
        case UnscheduledReason::ExceedsDailyWorkload: return "exceeds_daily_workload";
        case UnscheduledReason::NoFeasibleWindow: return "no_feasible_window";
        case UnscheduledReason::BlockedByPrerequisite: return "blocked_by_prerequisite";
        case UnscheduledReason::PrerequisiteCancelled: return "prerequisite_cancelled";
        case UnscheduledReason::UnknownPrerequisite: return "unknown_prerequisite";
        case UnscheduledReason::CircularDependency: return "circular_dependency";
        case UnscheduledReason::BlockedByCycle: return "blocked_by_cycle";
    }
    return "none";
}

const char* suggestedAction(UnscheduledReason r) {
    switch (r) {
        case UnscheduledReason::InvalidConstraints: return "Correct the task's fields and request a new schedule";
        case UnscheduledReason::OutsideHorizon: return "Extend config.end_date so the task's time range falls inside the horizon";
        case UnscheduledReason::DeadlineExceeded: return "Move the deadline later or start the schedule earlier";
        case UnscheduledReason::InsufficientTimeBeforeDeadline: return "Extend the deadline, free time before it, or reduce the duration";
        case UnscheduledReason::NoAvailableSlot: return "Extend the scheduling horizon, widen the availability windows or free up time";
        case UnscheduledReason::NonSplittableNoSlot: return "Allow splitting or free a longer continuous interval";
        case UnscheduledReason::MinChunkUnsatisfiable: return "Reduce min_chunk_min or free time in larger blocks";
        case UnscheduledReason::ExceedsDailyWorkload: return "Allow splitting or raise config.max_workload_min";
        case UnscheduledReason::NoFeasibleWindow: return "Widen earliest_start_min / latest_completion_min or the availability windows";
        case UnscheduledReason::BlockedByPrerequisite: return "Schedule or complete the prerequisite first";
        case UnscheduledReason::PrerequisiteCancelled: return "Remove the cancelled prerequisite or reinstate it";
        case UnscheduledReason::UnknownPrerequisite: return "Include the prerequisite task or remove the dependency";
        case UnscheduledReason::CircularDependency: return "Remove one dependency of the cycle";
        case UnscheduledReason::BlockedByCycle: return "Resolve the circular dependency this task waits on";
        case UnscheduledReason::None: break;
    }
    return "";
}

// ------------------------------------------------------------------------- SlotScanner

void SlotScanner::forEachFreeSlot(const Region& r, const std::function<bool(int, int, int)>& visit) const {
    const long long lo = (std::max)(0LL, r.lo);
    const long long hi = (std::min)(c_.horizonEndAbs(), r.hi);
    if (lo >= hi) return;
    const int firstDay = static_cast<int>(lo / kMinutesPerDay);
    const int lastDay = static_cast<int>((hi - 1) / kMinutesPerDay);
    for (int d = firstDay; d <= lastDay && d < c_.day_count; ++d) {
        if (!c_.isActiveDay(d)) continue;
        const long long base = static_cast<long long>(d) * kMinutesPerDay;
        for (const models::AvailabilityWindow& w : c_.windows) {
            const long long ws = (std::max)({base + w.start_min, base + r.tod_start, lo});
            const long long we = (std::min)({base + w.end_min, base + r.tod_end, hi});
            if (ws >= we) continue;
            // Occupied time near the window: events as they are, task sessions widened by the break.
            const int s = static_cast<int>(ws), e = static_cast<int>(we);
            std::vector<TimeInterval> occupied;
            for (const TimeInterval& o : tree_.findAllOverlaps(s - padding_, e + padding_)) {
                if (o.type == IntervalType::ScheduledTask && padding_ > 0)
                    occupied.emplace_back(o.start - padding_, o.end + padding_, o.id, o.type);
                else
                    occupied.push_back(o);
            }
            for (const TimeSlot& slot : algorithms::computeFreeSlots(occupied, s, e, 1))
                if (!visit(d, slot.start, slot.end)) return;
        }
    }
}

long long SlotScanner::freeMinutes(const Region& r) const {
    long long total = 0;
    forEachFreeSlot(r, [&](int, int s, int e) { total += e - s; return true; });
    return total;
}

// ------------------------------------------------------------------------- TaskPlacer

namespace {

// Size of the next session taken from a slot offering `usable` minutes, or 0 if the slot cannot
// be used. Rules: a session is never shorter than `min_chunk`; the work left afterwards is either
// 0 or at least `min_chunk` (so no invalid short final session is ever created).
int chooseChunk(int remaining, int usable, int min_chunk, bool split) {
    if (!split) return usable >= remaining ? remaining : 0;
    int c = (std::min)(remaining, usable);
    if (c < min_chunk) return 0;
    const int left = remaining - c;
    if (left > 0 && left < min_chunk) {
        c = remaining - min_chunk;
        if (c < min_chunk) return 0;
    }
    return c;
}

std::string clock(const SchedulingConstraints& c, long long abs) {
    const long long day = floorDiv(abs, kMinutesPerDay);
    return c.dateString(static_cast<int>(day)) + " " +
           models::formatHHmm(static_cast<int>(abs - day * kMinutesPerDay));
}

}  // namespace

TaskPlacement TaskPlacer::place(size_t index, long long ready_abs) {
    const models::Task& t = ctx_.task(index);
    const TaskInfo& info = ctx_.infos[index];
    const SchedulingConstraints& c = ctx_.constraints;

    TaskPlacement p;
    p.task_index = index;
    p.ready_abs = ready_abs;
    p.remaining_min = t.duration_min;

    const bool split = t.allow_split && c.allow_splitting;
    const int min_chunk = split ? t.min_chunk_min : t.duration_min;
    SlotScanner::Region region;
    region.lo = (std::max)(0LL, ready_abs);
    region.hi = c.horizonEndAbs();
    if (info.deadline_abs) region.hi = (std::min)(region.hi, *info.deadline_abs);
    region.tod_start = t.earliest_start_min.value_or(0);
    region.tod_end = t.latest_completion_min.value_or(kMinutesPerDay);

    const SlotScanner scanner(c, tree_, c.min_break_min);
    scanner.forEachFreeSlot(region, [&](int day, int slotStart, int slotEnd) {
        const int cap = c.max_workload_min - load_[static_cast<size_t>(day)];
        if (cap <= 0) return true;
        const int chunk = chooseChunk(p.remaining_min, (std::min)(slotEnd - slotStart, cap), min_chunk, split);
        if (chunk <= 0) return true;
        PlacedSession s;
        s.start_abs = slotStart;
        s.end_abs = slotStart + chunk;
        s.day_index = day;
        tree_.insert(TimeInterval(s.start_abs, s.end_abs, t.id, IntervalType::ScheduledTask));
        load_[static_cast<size_t>(day)] += chunk;
        p.scheduled_min += chunk;
        p.remaining_min -= chunk;
        p.sessions.push_back(s);
        return p.remaining_min > 0;
    });

    if (p.remaining_min == 0) {
        p.outcome = PlacementOutcome::Complete;
        p.completion_abs = p.sessions.back().end_abs;
        return p;
    }
    p.outcome = p.scheduled_min > 0 ? PlacementOutcome::Partial : PlacementOutcome::None;
    p.reason = diagnose(index, p, region.lo, region.hi, p.reason_text);
    return p;
}

// Explains WHY work is left over. Called after the sessions were committed, so the free time
// measured here is what is still free for this task.
UnscheduledReason TaskPlacer::diagnose(size_t index, const TaskPlacement& p, long long lo, long long hi,
                                       std::string& text) const {
    const models::Task& t = ctx_.task(index);
    const TaskInfo& info = ctx_.infos[index];
    const SchedulingConstraints& c = ctx_.constraints;
    const bool split = t.allow_split && c.allow_splitting;
    const std::string rem = std::to_string(p.remaining_min);
    const std::string what = p.scheduled_min > 0
                                 ? rem + " of " + std::to_string(t.duration_min) + " minutes could not be placed: "
                                 : "";
    const bool deadlineBinds = info.deadline_abs && *info.deadline_abs <= c.horizonEndAbs();

    if (p.ready_abs >= c.horizonEndAbs()) {
        text = "Prerequisites finish at " + clock(c, p.ready_abs) + ", after the scheduling horizon ends (" +
               c.end_date + ").";
        return UnscheduledReason::OutsideHorizon;
    }
    if (info.deadline_abs && *info.deadline_abs <= (std::max)(0LL, p.ready_abs)) {
        text = "Deadline exceeded: the deadline (" + t.deadline_date.value_or("") + " " +
               models::formatHHmm(t.deadline_min) + ") is not after " +
               (p.ready_abs > 0 ? "the time its prerequisites finish (" + clock(c, p.ready_abs) + ")"
                                : "the start of the scheduling horizon (" + c.start_date + ")") + ".";
        return UnscheduledReason::DeadlineExceeded;
    }
    if (lo >= hi) {
        text = "The task's permitted time range is empty inside the scheduling horizon.";
        return UnscheduledReason::OutsideHorizon;
    }
    if (!split && t.duration_min > c.max_workload_min) {
        text = "The task needs " + std::to_string(t.duration_min) + " continuous minutes but config.max_workload_min is " +
               std::to_string(c.max_workload_min) + " per day and the task cannot be split.";
        return UnscheduledReason::ExceedsDailyWorkload;
    }
    // Static shape: could the task ever fit its own time window, ignoring other commitments?
    int longestWindow = 0;
    for (const auto& w : c.windows)
        longestWindow = (std::max)(longestWindow, (std::min)(w.end_min, t.latest_completion_min.value_or(kMinutesPerDay)) -
                                                      (std::max)(w.start_min, t.earliest_start_min.value_or(0)));
    if (!split && longestWindow < t.duration_min) {
        text = "No availability window inside the task's allowed hours (earliest start/latest completion) is " +
               std::to_string(t.duration_min) + " minutes long, and the task cannot be split.";
        return UnscheduledReason::NoFeasibleWindow;
    }
    if (split && longestWindow < t.min_chunk_min) {
        text = "No availability window inside the task's allowed hours is as long as its minimum chunk (" +
               std::to_string(t.min_chunk_min) + " minutes).";
        return UnscheduledReason::NoFeasibleWindow;
    }

    SlotScanner::Region r;
    r.lo = lo; r.hi = hi;
    r.tod_start = t.earliest_start_min.value_or(0);
    r.tod_end = t.latest_completion_min.value_or(kMinutesPerDay);
    const long long freeLeft = SlotScanner(c, tree_, c.min_break_min).freeMinutes(r);

    if (freeLeft < p.remaining_min) {
        if (deadlineBinds) {
            text = what + "only " + std::to_string(freeLeft) + " free minutes remain before the deadline (" +
                   t.deadline_date.value_or("") + " " + models::formatHHmm(t.deadline_min) + "); " + rem + " are needed.";
            return UnscheduledReason::InsufficientTimeBeforeDeadline;
        }
        text = what + "no available time slot: only " + std::to_string(freeLeft) +
               " free minutes remain in the scheduling horizon (" + c.start_date + " to " + c.end_date + "); " + rem +
               " are needed.";
        return UnscheduledReason::NoAvailableSlot;
    }
    // Enough free minutes exist but not in usable shapes.
    if (!split) {
        text = "No continuous free interval of " + std::to_string(t.duration_min) +
               " minutes exists" + (deadlineBinds ? " before the deadline" : "") +
               " and the task cannot be split (fixed events, breaks or the daily workload limit fragment the time).";
        return UnscheduledReason::NonSplittableNoSlot;
    }
    text = what + "the remaining " + rem + " minutes cannot be placed in sessions of at least " +
           std::to_string(t.min_chunk_min) + " minutes (min_chunk_min) without violating the other constraints.";
    return UnscheduledReason::MinChunkUnsatisfiable;
}

}  // namespace chronoplan::scheduler
