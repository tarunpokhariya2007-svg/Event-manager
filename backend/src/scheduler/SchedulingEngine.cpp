#include "scheduler/SchedulingEngine.hpp"
#include <algorithm>
#include <chrono>
#include "algorithms/IdOrdering.hpp"
#include "scheduler/ScheduleBuilder.hpp"
#include "scheduler/SchedulingValidator.hpp"
#include "scheduler/TaskPlacement.hpp"

namespace chronoplan::scheduler {

using algorithms::IntervalTree;
using algorithms::SchedulingCandidate;
using models::Id;

namespace {

enum class State { Pending, Queued, Done, Failed, Completed, Cancelled };

std::string describe(const models::Task& t) {
    return "'" + t.title + "' (" + (t.id.numeric ? t.id.value : "\"" + t.id.value + "\"") + ")";
}

SchedulingCandidate toCandidate(const models::Task& t, const TaskInfo& info) {
    SchedulingCandidate c;
    c.task_id = t.id;
    c.priority = t.priority;
    c.deadline_date = t.deadline_date;
    c.deadline_min = t.deadline_min;
    c.earliest_start_min = t.earliest_start_min.value_or(0);
    c.duration_min = t.duration_min;
    c.deadline_abs_min = info.deadline_abs;
    return c;
}

}  // namespace

models::SchedulingResult SchedulingEngine::run(models::SchedulingRequest request) {
    const SchedulingContext ctx = SchedulingContext::build(std::move(request));
    return run(ctx);
}

models::SchedulingResult SchedulingEngine::run(const SchedulingContext& ctx) {
    const auto started = std::chrono::steady_clock::now();
    const size_t n = ctx.tasks.size();
    const SchedulingConstraints& c = ctx.constraints;

    // Working occupancy: fixed events go in BEFORE any task is placed.
    IntervalTree occupancy;
    for (const algorithms::TimeInterval& iv : ctx.fixed_intervals.toVector()) occupancy.insert(iv);
    std::vector<int> dayLoad(static_cast<size_t>(c.day_count), 0);
    TaskPlacer placer(ctx, occupancy, dayLoad);

    std::vector<State> state(n, State::Pending);
    std::vector<TaskPlacement> placements(n);
    std::vector<long long> completion(n, 0);
    auto fail = [&](size_t i, UnscheduledReason why, std::string text) {
        state[i] = State::Failed;
        placements[i].reason = why;
        placements[i].reason_text = std::move(text);
    };

    // ---- 1. classify tasks that can never be queued
    for (size_t i = 0; i < n; ++i) {
        const models::Task& t = ctx.tasks[i];
        const TaskInfo& info = ctx.infos[i];
        placements[i].task_index = i;
        if (info.state == TaskState::Completed) { state[i] = State::Completed; continue; }
        if (info.state == TaskState::Cancelled) { state[i] = State::Cancelled; continue; }
        if (info.invalid) {
            fail(i, UnscheduledReason::InvalidConstraints, "Invalid scheduling constraints: " + info.invalid_reason);
            continue;
        }
        placements[i].remaining_min = t.duration_min;
        if (info.in_cycle) {
            std::string members;
            for (const Id& m : info.cycle_members) members += (members.empty() ? "" : ", ") + (m.numeric ? m.value : "\"" + m.value + "\"");
            fail(i, UnscheduledReason::CircularDependency,
                 "Circular dependency: the task is part of a dependency cycle (" + members + ") and can never become ready.");
        } else if (info.downstream_of_cycle) {
            std::string by;
            for (const size_t p : info.prerequisites)
                if (ctx.infos[p].in_cycle || ctx.infos[p].downstream_of_cycle) { by = describe(ctx.tasks[p]); break; }
            fail(i, UnscheduledReason::BlockedByCycle,
                 "Blocked: it depends on " + by + ", which is on or downstream of a circular dependency.");
        } else if (!info.unknown_prerequisites.empty()) {
            const Id& u = info.unknown_prerequisites.front();
            fail(i, UnscheduledReason::UnknownPrerequisite,
                 "Prerequisite " + (u.numeric ? u.value : "\"" + u.value + "\"") + " is not part of the task list.");
        }
    }

    // ---- 2. eligibility helpers
    auto satisfied = [&](size_t p) { return state[p] == State::Done || state[p] == State::Completed; };
    auto allPrerequisitesSatisfied = [&](size_t i) {
        for (const size_t p : ctx.infos[i].prerequisites) if (!satisfied(p)) return false;
        return true;
    };
    auto readyTime = [&](size_t i) {
        long long t = 0;
        for (const size_t p : ctx.infos[i].prerequisites) t = (std::max)(t, completion[p]);
        return t;
    };

    ObjectiveQueue queue(ObjectiveRanksBefore{c.objective});
    std::vector<long long> ready(n, 0);
    auto enqueue = [&](size_t i) {
        ready[i] = readyTime(i);
        state[i] = State::Queued;
        queue.push(toCandidate(ctx.tasks[i], ctx.infos[i]));
    };
    for (size_t i = 0; i < n; ++i)
        if (state[i] == State::Pending && allPrerequisitesSatisfied(i)) enqueue(i);

    // ---- 3. list scheduling
    int rank = 0;
    while (auto next = queue.pop()) {
        const size_t i = ctx.task_index.at(next->task_id.key());
        TaskPlacement p = placer.place(i, ready[i]);
        p.rank = ++rank;
        placements[i] = std::move(p);
        if (placements[i].outcome == PlacementOutcome::Complete) {
            state[i] = State::Done;
            completion[i] = *placements[i].completion_abs;
            // Newly eligible dependents (all prerequisites must be resolved, not just this one).
            for (const Id& d : ctx.graph.getDependents(ctx.tasks[i].id)) {
                const size_t di = ctx.task_index.at(d.key());
                if (state[di] == State::Pending && allPrerequisitesSatisfied(di)) enqueue(di);
            }
        } else {
            state[i] = State::Failed;
        }
    }

    // ---- 4. whatever is still Pending is blocked by an unresolved prerequisite
    for (size_t i = 0; i < n; ++i) {
        if (state[i] != State::Pending) continue;
        std::vector<size_t> prereqs = ctx.infos[i].prerequisites;
        std::sort(prereqs.begin(), prereqs.end(), [&](size_t a, size_t b) {
            return algorithms::compareIds(ctx.tasks[a].id, ctx.tasks[b].id) < 0;
        });
        for (const size_t p : prereqs) {
            if (satisfied(p)) continue;
            if (state[p] == State::Cancelled) {
                fail(i, UnscheduledReason::PrerequisiteCancelled, "Blocked: prerequisite " + describe(ctx.tasks[p]) + " is cancelled.");
            } else {
                const std::string why = state[p] == State::Failed ? std::string("it could not be fully scheduled (") +
                                                                          reasonCode(placements[p].reason) + ")"
                                                                  : "it is itself blocked";
                fail(i, UnscheduledReason::BlockedByPrerequisite,
                     "Blocked by prerequisite " + describe(ctx.tasks[p]) + ": " + why + ".");
            }
            break;
        }
        if (state[i] == State::Pending) fail(i, UnscheduledReason::BlockedByPrerequisite, "Blocked by an unresolved prerequisite.");
    }
    // A blocked task's reason may name a prerequisite that was resolved later in the loop above;
    // re-label "itself blocked" prerequisites now that every state is final.
    for (size_t i = 0; i < n; ++i) {
        if (placements[i].reason != UnscheduledReason::BlockedByPrerequisite) continue;
        for (const size_t p : ctx.infos[i].prerequisites) {
            if (satisfied(p) || state[p] != State::Failed) continue;
            if (placements[p].reason == UnscheduledReason::BlockedByPrerequisite ||
                placements[p].reason == UnscheduledReason::BlockedByCycle) {
                placements[i].reason_text = "Blocked by prerequisite " + describe(ctx.tasks[p]) +
                                            ", which is itself blocked (" + reasonCode(placements[p].reason) + ").";
                break;
            }
        }
    }

    models::SchedulingResult result = ScheduleBuilder::build(ctx, placements, occupancy, rank);

    // ---- 5. independent validation: never trust the placer
    result.validation_errors = SchedulingValidator::validate(ctx, result);
    if (!result.validation_errors.empty()) result.status = models::ResultStatus::Infeasible;

    result.solver_duration_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    return result;
}

}  // namespace chronoplan::scheduler
