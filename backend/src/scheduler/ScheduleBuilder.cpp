#include "scheduler/ScheduleBuilder.hpp"
#include <algorithm>
#include <cmath>
#include "algorithms/IdOrdering.hpp"

namespace chronoplan::scheduler {

using models::Id;
using models::TimelineEntry;

namespace {

double round2(double v) { return std::round(v * 100.0) / 100.0; }

std::string shortId(const Id& id) { return id.numeric ? id.value : "\"" + id.value + "\""; }

}  // namespace

models::SchedulingResult ScheduleBuilder::build(const SchedulingContext& ctx, const std::vector<TaskPlacement>& placements,
                                                const algorithms::IntervalTree& occupancy, int attempted) {
    const SchedulingConstraints& c = ctx.constraints;
    models::SchedulingResult r;
    r.target_date = c.start_date;
    r.start_date = c.start_date;
    r.end_date = c.end_date;
    r.objective = toString(c.objective);
    r.conflicts = ctx.conflicts;
    r.warnings = ctx.warnings;
    if (ctx.events_outside_horizon > 0)
        r.warnings.push_back(std::to_string(ctx.events_outside_horizon) +
                             " event(s) outside the scheduling horizon were ignored");
    for (const auto& cf : ctx.conflicts) (void)cf;
    if (!ctx.conflicts.empty())
        r.warnings.push_back(std::to_string(ctx.conflicts.size()) + " overlapping event pair(s) reported in conflicts");

    // ---- timeline: events first (they never move), then task sessions
    struct Row { int day; TimelineEntry entry; Id ref; };
    std::vector<Row> rows;
    for (const models::Event& e : ctx.events) {
        TimelineEntry t;
        t.date = e.date;
        t.type = models::TimelineType::Event;
        t.reference_id = e.id;
        t.title = e.title;
        t.start_min = e.start_min;
        t.end_min = e.end_min;
        t.priority = e.priority;
        t.status = models::TimelineStatus::Fixed;
        t.explanation = e.isFlexible()
                            ? "Flexible event kept at its stored time; the scheduler never moves events."
                            : "Fixed event; not moved.";
        rows.push_back({ctx.dayIndexOf(e.date), std::move(t), e.id});
    }
    for (size_t i = 0; i < ctx.tasks.size(); ++i) {
        const TaskPlacement& p = placements[i];
        const models::Task& task = ctx.tasks[i];
        const int n = static_cast<int>(p.sessions.size());
        for (int k = 0; k < n; ++k) {
            const PlacedSession& s = p.sessions[static_cast<size_t>(k)];
            TimelineEntry t;
            t.date = c.dateString(s.day_index);
            t.type = models::TimelineType::Task;
            t.reference_id = task.id;
            t.title = task.title;
            t.start_min = s.start_min();
            t.end_min = s.end_min();
            t.priority = task.priority;
            t.status = models::TimelineStatus::Scheduled;
            t.session_index = k + 1;
            t.session_count = n;
            std::string why = "Placed #" + std::to_string(p.rank) + " of " + std::to_string(attempted) +
                              " by the '" + toString(c.objective) + "' objective (priority " +
                              std::to_string(task.priority) + "), in the earliest free slot";
            if (p.ready_abs > 0) why += " after its prerequisites finish";
            why += ". ";
            if (n > 1) why += "Session " + std::to_string(k + 1) + " of " + std::to_string(n) + ". ";
            if (p.outcome == PlacementOutcome::Partial)
                why += std::to_string(p.remaining_min) + " minutes could not be placed (" + reasonCode(p.reason) + ").";
            while (!why.empty() && why.back() == ' ') why.pop_back();
            t.explanation = why;
            rows.push_back({s.day_index, std::move(t), task.id});
        }
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.day != b.day) return a.day < b.day;
        if (a.entry.start_min != b.entry.start_min) return a.entry.start_min < b.entry.start_min;
        if (a.entry.end_min != b.entry.end_min) return a.entry.end_min < b.entry.end_min;
        if (a.entry.type != b.entry.type) return a.entry.type == models::TimelineType::Event;   // events first
        return algorithms::compareIds(a.ref, b.ref) < 0;
    });
    int n = 0;
    for (Row& row : rows) {
        row.entry.id = "tl-" + std::to_string(++n);
        r.timeline.push_back(std::move(row.entry));
    }

    // ---- free slots left after placement (plain free time, no break padding)
    {
        int slotNo = 0;
        SlotScanner scanner(c, occupancy, 0);
        SlotScanner::Region all;
        all.lo = 0;
        all.hi = c.horizonEndAbs();
        scanner.forEachFreeSlot(all, [&](int day, int s, int e) {
            models::Slot slot;
            slot.id = "slot-" + std::to_string(++slotNo);
            slot.date = c.dateString(day);
            slot.start_min = s - day * kMinutesPerDay;
            slot.end_min = e - day * kMinutesPerDay;
            slot.label = "Free";
            r.available_slots.push_back(std::move(slot));
            return true;
        });
    }

    // ---- unscheduled tasks, per-task counters
    models::SchedulingStatistics& st = r.statistics;
    for (size_t i = 0; i < ctx.tasks.size(); ++i) {
        const models::Task& task = ctx.tasks[i];
        const TaskInfo& info = ctx.infos[i];
        const TaskPlacement& p = placements[i];
        if (info.state != TaskState::Active) { ++st.tasks_skipped_terminal; continue; }
        ++st.tasks_considered;
        if (!info.invalid) st.total_requested_min += task.duration_min;
        st.total_scheduled_min += p.scheduled_min;
        st.session_count += static_cast<int>(p.sessions.size());
        switch (p.outcome) {
            case PlacementOutcome::Complete: ++st.tasks_scheduled; break;
            case PlacementOutcome::Partial: ++st.tasks_partially_scheduled; break;
            case PlacementOutcome::None: ++st.tasks_unscheduled; break;
        }
        if (p.outcome != PlacementOutcome::Complete) {
            models::UnscheduledTask u;
            u.task_id = task.id;
            u.title = task.title;
            u.remaining_min = info.invalid ? 0 : p.remaining_min;
            u.reason_code = reasonCode(p.reason);
            u.reason = p.reason_text.empty() ? "Task could not be scheduled." : p.reason_text;
            u.suggested_action = suggestedAction(p.reason);
            r.unscheduled_tasks.push_back(std::move(u));
        }
    }
    st.total_remaining_min = st.total_requested_min - st.total_scheduled_min;
    st.fixed_events = static_cast<int>(ctx.events.size());

    // ---- deadline analysis
    DeadlineAnalysis analysis = DeadlineAnalyzer::analyze(ctx, placements, occupancy);
    r.tasks_at_risk = std::move(analysis.at_risk);
    st.tasks_meeting_deadline = analysis.tasks_meeting_deadline;

    // ---- capacity and utilisation (events counted once, never as task minutes)
    {
        long long windowMin = 0;
        for (int d = 0; d < c.day_count; ++d)
            if (c.isActiveDay(d)) windowMin += c.windowMinutesPerDay();
        const SlotScanner eventsOnly(c, ctx.fixed_intervals, 0);
        SlotScanner::Region all;
        all.lo = 0;
        all.hi = c.horizonEndAbs();
        const long long freeOfEvents = eventsOnly.freeMinutes(all);
        st.total_window_min = static_cast<int>(windowMin);
        st.total_available_min = static_cast<int>(freeOfEvents);
        st.fixed_event_window_min = static_cast<int>(windowMin - freeOfEvents);
        st.total_occupied_min = st.total_scheduled_min + st.fixed_event_window_min;
        st.utilization_pct = st.total_available_min > 0
                                 ? round2(100.0 * st.total_scheduled_min / st.total_available_min)
                                 : 0.0;
    }
    r.total_allocated_min = st.total_scheduled_min;
    r.total_available_min = st.total_available_min;

    // ---- overall status
    const bool anyUnplaced = st.tasks_unscheduled > 0 || st.tasks_partially_scheduled > 0;
    // Unresolved overlapping events (reported in `conflicts`, never moved) mean the schedule is not
    // fully successful even when every task was placed.
    const bool hasEventConflicts = !ctx.conflicts.empty();
    if (!anyUnplaced && !hasEventConflicts) r.status = models::ResultStatus::Success;
    else if (anyUnplaced && st.total_scheduled_min == 0) r.status = models::ResultStatus::Infeasible;
    else r.status = models::ResultStatus::Partial;
    (void)shortId;
    return r;
}

}  // namespace chronoplan::scheduler
