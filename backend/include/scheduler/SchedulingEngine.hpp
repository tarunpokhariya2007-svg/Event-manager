#pragma once
// SchedulingEngine: deterministic list scheduling over the Phase 3 data structures.
//
//   DependencyGraph  -> which tasks are eligible (all prerequisites resolved, no cycle)
//   ObjectiveQueue   -> which eligible task is placed next (custom binary-heap PriorityQueue)
//   IntervalTree     -> occupied time (fixed events + sessions already placed), free-slot search
//
// The engine knows nothing about Crow or SQLite; services::SchedulingService feeds it.
#include "models/Schedule.hpp"
#include "scheduler/SchedulingContext.hpp"

namespace chronoplan::scheduler {

class SchedulingEngine {
public:
    // Builds the context (throws models::ValidationError for an invalid config/event), schedules,
    // validates the result independently and returns it. Cycles and unplaceable tasks are NOT
    // errors: they appear in unscheduled_tasks / tasks_at_risk.
    static models::SchedulingResult run(models::SchedulingRequest request);
    static models::SchedulingResult run(const SchedulingContext& ctx);
};

}  // namespace chronoplan::scheduler
