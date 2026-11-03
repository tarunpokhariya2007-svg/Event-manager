#pragma once
// DeadlineAnalyzer: classifies deadline risk AFTER placement. It distinguishes
//   deadline_risk         placed, but little free working time is left before the deadline
//   hard_infeasible       the deadline already passed / the task can never satisfy its constraints
//   insufficient_capacity not enough usable time before the (in-horizon) deadline
//   dependency_blocked    a prerequisite (or a cycle) keeps the task from being placed
// Tasks without a deadline are never reported as at risk (they appear in unscheduled_tasks only).
#include <algorithm>
#include <vector>
#include "algorithms/IntervalTree.hpp"
#include "models/Schedule.hpp"
#include "scheduler/TaskPlacement.hpp"

namespace chronoplan::scheduler {

struct DeadlineAnalysis {
    std::vector<models::TaskAtRisk> at_risk;   // most severe first, then by deadline, then id
    int tasks_meeting_deadline = 0;            // fully placed tasks that have a deadline (all finish on time)
};

class DeadlineAnalyzer {
public:
    // A fully placed task is "at risk" when the free working time left between its completion and
    // its deadline is below max(30, ceil(duration/4)) minutes.
    static int riskBufferThreshold(const models::Task& t) { return (std::max)(30, (t.duration_min + 3) / 4); }

    static DeadlineAnalysis analyze(const SchedulingContext& ctx, const std::vector<TaskPlacement>& placements,
                                    const algorithms::IntervalTree& finalOccupancy);
};

}  // namespace chronoplan::scheduler
