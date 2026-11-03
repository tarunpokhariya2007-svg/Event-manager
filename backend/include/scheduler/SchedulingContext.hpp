#pragma once
// SchedulingContext: everything the engine needs, prepared once per request and independent of
// Crow and SQLite. It owns normalised copies of the tasks/events, the Phase 3 DependencyGraph,
// the IntervalTree of fixed events (absolute minutes) and the per-task facts (state, deadline,
// cycle membership, prerequisites).
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "algorithms/DependencyGraph.hpp"
#include "algorithms/IntervalTree.hpp"
#include "models/Schedule.hpp"
#include "scheduler/SchedulingConstraints.hpp"

namespace chronoplan::scheduler {

// Task::status is an opaque token in the Phase 2 contract. Phase 4 recognises these tokens
// (case-insensitive, surrounding spaces ignored); anything else, or no status, means Active.
//   completed: "completed", "complete", "done", "finished"
//   cancelled: "cancelled", "canceled"
enum class TaskState { Active, Completed, Cancelled };
TaskState classifyStatus(const std::optional<std::string>& status);

struct TaskInfo {
    TaskState state = TaskState::Active;
    std::optional<long long> deadline_abs;       // minutes from horizon start; nullopt = no deadline
    bool invalid = false;                        // constraints failed Task::validate()
    std::string invalid_reason;
    bool in_cycle = false;                       // member of a dependency cycle
    bool downstream_of_cycle = false;            // depends (transitively) on a cycle member
    std::vector<models::Id> cycle_members;       // the cycle component (when in_cycle)
    std::vector<size_t> prerequisites;           // indices into SchedulingContext::tasks
    std::vector<models::Id> unknown_prerequisites;   // referenced ids that are not in the task list
};

struct SchedulingContext {
    SchedulingConstraints constraints;
    std::vector<models::Task> tasks;             // ids assigned (see build)
    std::vector<TaskInfo> infos;                 // parallel to tasks
    std::unordered_map<std::string, size_t> task_index;   // Id::key() -> index
    std::vector<models::Event> events;           // only events inside the horizon, sorted
    int events_outside_horizon = 0;
    algorithms::DependencyGraph graph;           // prerequisite -> dependent
    algorithms::IntervalTree fixed_intervals;    // events in ABSOLUTE minutes, IntervalType::Event
    std::vector<models::Conflict> conflicts;     // overlapping events (reported, never moved)
    std::vector<std::string> warnings;

    const models::Task& task(size_t i) const { return tasks[i]; }
    long long absStart(const models::Event& e) const;   // event start as absolute minutes
    int dayIndexOf(const std::string& date) const;      // may be outside [0, day_count)

    // Normalises the request: assigns ids to tasks/events that have none (the next numeric id,
    // as the repositories do), keeps only events inside the horizon, builds the graph, finds
    // cycles, inserts the fixed events into the interval tree and records event conflicts.
    // Throws models::ValidationError for an invalid config or an invalid event.
    static SchedulingContext build(models::SchedulingRequest request);
};

}  // namespace chronoplan::scheduler
