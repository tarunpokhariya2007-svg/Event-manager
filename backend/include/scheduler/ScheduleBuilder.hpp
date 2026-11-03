#pragma once
// ScheduleBuilder: turns the engine's placements into the public models::SchedulingResult
// (sorted timeline, free slots, unscheduled tasks, tasks at risk, conflicts, statistics).
#include <vector>
#include "algorithms/IntervalTree.hpp"
#include "models/Schedule.hpp"
#include "scheduler/DeadlineAnalyzer.hpp"
#include "scheduler/TaskPlacement.hpp"

namespace chronoplan::scheduler {

class ScheduleBuilder {
public:
    // `placements` is parallel to ctx.tasks. `occupancy` = fixed events + every placed session.
    // `attempted` = number of tasks the engine tried to place (rank denominator in explanations).
    static models::SchedulingResult build(const SchedulingContext& ctx, const std::vector<TaskPlacement>& placements,
                                          const algorithms::IntervalTree& occupancy, int attempted);
};

}  // namespace chronoplan::scheduler
