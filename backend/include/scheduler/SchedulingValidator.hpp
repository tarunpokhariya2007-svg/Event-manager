#pragma once
// SchedulingValidator: checks a SchedulingResult WITHOUT trusting the engine. It re-derives every
// session from the result's timeline (date + start/end) and checks the hard constraints against the
// context's tasks, events and constraints only (not the engine's internal state or interval tree).
//
// Violation codes: invalid_entry (id/date/boundaries), outside_horizon, outside_availability,
// task_overlap, break_violation, event_overlap, event_modified, early_start, late_completion,
// deadline_violation, dependency_order, prerequisite_unscheduled, task_split, chunk_too_short,
// duration_exceeded, workload_exceeded, terminal_task_scheduled, cyclic_task_scheduled, unknown_task.
#include <vector>
#include "models/Schedule.hpp"
#include "scheduler/SchedulingContext.hpp"

namespace chronoplan::scheduler {

class SchedulingValidator {
public:
    static std::vector<models::ScheduleViolation> validate(const SchedulingContext& ctx,
                                                           const models::SchedulingResult& result);
};

}  // namespace chronoplan::scheduler
