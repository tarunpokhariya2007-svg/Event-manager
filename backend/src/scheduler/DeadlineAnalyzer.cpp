#include "scheduler/DeadlineAnalyzer.hpp"
#include <algorithm>
#include "algorithms/IdOrdering.hpp"

namespace chronoplan::scheduler {

using models::RiskLevel;
using models::TaskAtRisk;

namespace {

std::string deadlineText(const models::Task& t) {
    return *t.deadline_date + "T" + models::formatHHmm(t.deadline_min);
}

std::string completionText(const SchedulingConstraints& c, int abs) {
    const int day = abs / kMinutesPerDay;
    return dateTimeString(c.dateString(day), abs - day * kMinutesPerDay);
}

}  // namespace

DeadlineAnalysis DeadlineAnalyzer::analyze(const SchedulingContext& ctx, const std::vector<TaskPlacement>& placements,
                                           const algorithms::IntervalTree& occupancy) {
    DeadlineAnalysis out;
    const SchedulingConstraints& c = ctx.constraints;
    const SlotScanner scanner(c, occupancy, 0);

    for (size_t i = 0; i < ctx.tasks.size(); ++i) {
        const models::Task& t = ctx.tasks[i];
        const TaskInfo& info = ctx.infos[i];
        const TaskPlacement& pl = placements[i];
        if (info.state != TaskState::Active || info.invalid || !info.deadline_abs) continue;
        const long long deadline = *info.deadline_abs;
        // A deadline at 24:00 of the last day still lies inside the horizon.
        const bool inHorizon = deadline <= c.horizonEndAbs();

        TaskAtRisk r;
        r.task_id = t.id;
        r.title = t.title;
        r.deadline = deadlineText(t);

        if (pl.outcome == PlacementOutcome::Complete) {
            ++out.tasks_meeting_deadline;
            if (!inHorizon) continue;                  // finishes inside the horizon, deadline is later
            r.estimated_completion = completionText(c, *pl.completion_abs);
            SlotScanner::Region region;
            region.lo = *pl.completion_abs;
            region.hi = deadline;
            const int free = static_cast<int>(scanner.freeMinutes(region));
            if (free >= riskBufferThreshold(t)) continue;
            r.risk_level = RiskLevel::AtRisk;
            r.category = "deadline_risk";
            r.required_min = t.duration_min;
            r.buffer_min = free;
            r.available_min = t.duration_min + free;
            r.reason = "Scheduled to finish " + r.estimated_completion->substr(0, 10) + " " +
                       r.estimated_completion->substr(11) + "; only " + std::to_string(free) +
                       " free working minutes remain before the deadline.";
            out.at_risk.push_back(std::move(r));
            continue;
        }

        // Not (fully) placed. Estimated completion is unknown.
        const UnscheduledReason why = pl.reason;
        SlotScanner::Region region;
        region.lo = (std::max)(0LL, pl.ready_abs);
        region.hi = (std::min)(c.horizonEndAbs(), deadline);
        region.tod_start = t.earliest_start_min.value_or(0);
        region.tod_end = t.latest_completion_min.value_or(kMinutesPerDay);
        const int free = region.lo < region.hi ? static_cast<int>(scanner.freeMinutes(region)) : 0;
        const int required = pl.remaining_min > 0 ? pl.remaining_min : t.duration_min;
        r.required_min = required;
        r.available_min = pl.scheduled_min + free;
        r.buffer_min = free - required;
        r.reason = pl.reason_text;

        switch (why) {
            case UnscheduledReason::DeadlineExceeded:
                r.risk_level = RiskLevel::ImpossibleWithoutRescheduling;
                r.category = "hard_infeasible";
                break;
            case UnscheduledReason::NoFeasibleWindow:
            case UnscheduledReason::ExceedsDailyWorkload:
                r.risk_level = RiskLevel::ImpossibleWithoutRescheduling;
                r.category = "hard_infeasible";
                break;
            case UnscheduledReason::CircularDependency:
            case UnscheduledReason::BlockedByCycle:
                r.risk_level = RiskLevel::ImpossibleWithoutRescheduling;
                r.category = "dependency_blocked";
                break;
            case UnscheduledReason::BlockedByPrerequisite:
            case UnscheduledReason::PrerequisiteCancelled:
            case UnscheduledReason::UnknownPrerequisite:
                if (!inHorizon) continue;              // deadline lies beyond this horizon: not at risk here
                r.risk_level = RiskLevel::AtRisk;
                r.category = "dependency_blocked";
                break;
            default:   // capacity-type reasons
                if (!inHorizon) continue;              // not a deadline problem within this horizon
                r.risk_level = RiskLevel::ImpossibleWithoutRescheduling;
                r.category = "insufficient_capacity";
                break;
        }
        out.at_risk.push_back(std::move(r));
    }

    std::stable_sort(out.at_risk.begin(), out.at_risk.end(), [&](const TaskAtRisk& a, const TaskAtRisk& b) {
        if (a.risk_level != b.risk_level) return a.risk_level > b.risk_level;   // Impossible > AtRisk
        if (a.deadline != b.deadline) return a.deadline < b.deadline;           // ISO text sorts correctly
        return algorithms::compareIds(a.task_id, b.task_id) < 0;
    });
    return out;
}

}  // namespace chronoplan::scheduler
