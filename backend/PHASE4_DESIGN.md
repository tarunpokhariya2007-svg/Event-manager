# ChronoPlan Phase 4 — Intelligent Scheduling Engine

Scope: a deterministic, constraint-aware list scheduler that turns tasks, events, dependencies and
availability into a proposed timetable, validates it independently, and exposes it as
`POST /api/v1/schedule/optimize` (a read-only **preview**). No machine learning, no external
services, no database schema change.

## 1. Architecture

```
React (future) -> Crow route (ScheduleRoutes.cpp)
                      -> services::SchedulingService      (only layer that reads repositories)
                            -> SchedulingDataProvider      (data_source = "database": 2 queries total)
                            -> scheduler::SchedulingContext (normalise, graph, cycles, event tree, conflicts)
                            -> scheduler::SchedulingEngine
                                  DependencyGraph  : eligibility, cycle exclusion        (Phase 3)
                                  ObjectiveQueue   : PriorityQueue<SchedulingCandidate>   (Phase 3 heap)
                                  IntervalTree     : fixed events + placed sessions       (Phase 3 AVL)
                                  TaskPlacer / SlotScanner : earliest-fit placement, splitting
                                  ScheduleBuilder + DeadlineAnalyzer : result, statistics, risks
                                  SchedulingValidator : independent re-check of the result
                      <- SchedulingResult.toApiJson()
```

`include/scheduler/*`, `src/scheduler/*` form the library `chronoplan_scheduler`. It depends only on
`chronoplan_algorithms` and the header-only models — **no Crow, no SQLite** (CMake enforces this).

| File | Role |
|---|---|
| `CalendarDate.hpp` | day numbers, weekdays, date formatting, floor division |
| `SchedulingObjective.*` | the five objectives = the ranking rule of the PriorityQueue |
| `SchedulingConstraints.*` | validated horizon/windows/weekdays/break/workload from `SchedulingConfig` |
| `SchedulingContext.*` | everything per request: tasks, events (in horizon), graph, cycles, event tree, conflicts |
| `TaskPlacement.*` | `SlotScanner` (free slots) and `TaskPlacer` (earliest fit, splitting, failure diagnosis) |
| `DeadlineAnalyzer.*` | `tasks_at_risk` classification |
| `ScheduleBuilder.*` | timeline, free slots, unscheduled tasks, statistics, overall status |
| `SchedulingValidator.*` | independent hard-constraint validation |
| `SchedulingEngine.*` | orchestration of the list-scheduling loop |
| `services/SchedulingService.*` | request modes, input rejection, engine call, refuses invalid schedules |

## 2. Request format

Unchanged Phase 1 body (`config`, `fixed_events`, `flexible_tasks`) plus optional additions. See
`API_CONTRACT.md` §5 and `examples/optimize_request.json`.

`data_source` (new, optional): `"request"` (default; tasks/events come from the body, the database is
never read) or `"database"` (all stored tasks with their stored prerequisites and every stored event inside
the horizon are loaded once; `flexible_tasks`/`fixed_events` must then be absent or empty, otherwise `400`).

## 3. Scheduling configuration

| Field | Meaning in Phase 4 |
|---|---|
| `target_date` | first day of the horizon |
| `end_date` (new) | last day, inclusive; default `target_date`; at most 366 days; not before `target_date` |
| `availability_windows` (new) | daily windows, e.g. 540–720 and 780–1080. Must not overlap; touching windows are merged. **Replace** `work_window_*` when present |
| `work_window_start_min/_end_min` | single daily window (default 510–1110, i.e. 08:30–18:30) when no `availability_windows` |
| `working_weekdays` (new) | `["mon".."sun"]` (or full names); default = all seven days |
| `min_break_min` | minimum gap between any two **task sessions**. Fixed events are not padded |
| `max_workload_min` | hard cap on task minutes per day |
| `allow_splitting` | global switch, ANDed with each task's `allow_split` |
| `objective` | `balanced`, `priority`, `deadline`, `compact`, `edf` |

Defaults are the Phase 1 defaults (08:30–18:30, break 30, workload 450, balanced); the scheduler
never assumes 24-hour availability. Every Phase 4 field is optional, so a Phase 1 config still means
"one day".

## 4. Available time

For each day of the horizon on a working weekday, each window is a candidate region. Inside a region
`SlotScanner` asks the IntervalTree for overlaps (`findAllOverlaps`), widens placed-task intervals by
`min_break_min`, and calls `computeFreeSlots`. The task's own `earliest_start_min` / `latest_completion_min`
clip the region **on every day** (they are times of day, as in the model), as do the earliest start
imposed by prerequisites and the deadline.

## 5. Fixed events

All events inside the horizon are inserted into the IntervalTree (absolute minutes) before any task is
placed. Events outside the horizon are ignored (counted in a warning). Overlapping events are reported
in `conflicts` (`cf-N`, with the overlap range) and both stay occupied. Touching events are not conflicts
(half-open intervals). Invalid events are rejected (`400`).
**Flexible events** (`is_flexible: true`): the existing model has no rescheduling semantics, so they are
treated exactly like fixed events — their stored interval stays occupied and is never moved. Nothing in
the engine modifies an event.

## 6. Eligibility

A task is placed only when it is *active*, valid, and every prerequisite is resolved.
* Status tokens (Phase 2 left `status` opaque; **assumption**, case-insensitive): `completed`, `complete`,
  `done`, `finished` = completed; `cancelled`, `canceled` = cancelled; anything else/none = active.
  Terminal tasks are never placed and are not "unscheduled" (`tasks_skipped_terminal`).
  A completed prerequisite is satisfied; a **cancelled** prerequisite blocks (`prerequisite_cancelled`).
* **Cycles.** The graph is analysed with Tarjan SCC (`DependencyGraph::cyclicComponents`). Tasks on a
  cycle (`circular_dependency`) and tasks downstream of one (`blocked_by_cycle`) never enter the queue —
  even if a cycle member is marked completed. This is the Phase 3 fix: `readyTasks()` looked only at
  direct prerequisites, so in `A <-> B` with `A` completed, `B` looked ready.
  `SchedulingDataProvider::buildPriorityQueue` now uses `readyTasksExcludingCycles()`;
  `readyTasks()` itself is unchanged (its tests are untouched).
  Independent tasks are scheduled normally. At API level a cycle in the *input* is rejected with `409
  circular_dependency`; the engine itself degrades gracefully (used for stored data and tests).
* Unknown prerequisite ids: `400` at the API; engine-level reason `unknown_prerequisite`.

## 7. Objectives (priority selection)

The ready tasks live in the Phase 3 `PriorityQueue`, instantiated with `ObjectiveRanksBefore`.
Higher `priority` = more important (5). All rules end with the task id (total, deterministic order).

| Objective | Order |
|---|---|
| `priority` | Phase 3 `compareCandidates`: priority ↓, has-deadline, earlier deadline, earlier earliest start, longer duration, id |
| `deadline` | has deadline first; earlier deadline **date**; then priority ↓; then exact deadline minute; earliest start; longer; id |
| `edf` | has deadline first; exact earliest deadline (priority ignored); id |
| `balanced` | score = `priority*100 + urgency` (urgency = 300 − 30 per whole day to the deadline, min 0, 0 without deadline); score ↓, then `priority` rule |
| `compact` | shorter duration first, then `priority` rule |

Objectives only change the order tasks are *offered*; hard constraints hold for all of them.

## 8. Placement algorithm (serial list scheduling, earliest fit)

```
build context; occupancy tree <- fixed events
queue <- tasks whose prerequisites are all satisfied (completed or none), not cycle-affected
while queue not empty:
    task <- queue.pop()                         # ranked by the objective
    place(task, ready = latest completion of its prerequisites)
    if fully placed: mark done; push every dependent whose prerequisites are now ALL done
    else:            mark failed (partial sessions stay)  -> dependents become blocked
remaining pending tasks -> blocked_by_prerequisite / prerequisite_cancelled
result <- build; validate independently
```
`place` walks days then windows chronologically, takes free slots clipped to
`[ready, deadline]` and the task's time-of-day limits, caps each day by `max_workload_min`, and puts the
next session at the **start** of the first usable slot, inserting it into the tree.

## 9. Splitting

* Effective split = `task.allow_split && config.allow_splitting`.
* Non-splittable: one block of `duration_min` inside a single free slot (never across events, breaks,
  window ends or days).
* Splittable: a session from a slot is `min(remaining, usable)`. Never shorter than `min_chunk_min`. If
  taking it would leave `0 < left < min_chunk_min`, the session shrinks to `remaining − min_chunk_min`
  (if that is still ≥ `min_chunk_min`, else the slot is skipped). So no invalid short final session is
  ever created. Whatever cannot be placed that way is reported as `remaining_min` with a reason.
* Total scheduled minutes never exceed `duration_min`; sessions never overlap; each session stays inside one window.

## 10. Dependency timing

A task's completion = end of its **final** session. A dependent may not start before the latest completion
of *all* its prerequisites (`ready` time); completed prerequisites add no constraint. A partially placed or
unplaced prerequisite blocks its dependents.

## 11. Deadlines and risk (`DeadlineAnalyzer`)

* **Deadline policy:** a deadline exists only if `deadline_date` is set; it is minute `deadline_min` of that
  date (default 1440 = the **end of that day**, written `T24:00`). Work must *end* at or before it.
  A date-only deadline is therefore never silently turned into another time.
* `earliest_start_min` / `latest_completion_min`: time-of-day limits applied to every day.
* A deadline before the horizon / before the prerequisites finish → `deadline_exceeded` (`hard_infeasible`).
* `tasks_at_risk[].category`: `deadline_risk` (placed, but free working time between completion and deadline
  `< max(30, ceil(duration/4))`; level `at_risk`), `hard_infeasible`, `insufficient_capacity` (not enough usable
  time before an in-horizon deadline), `dependency_blocked`. Tasks without a deadline are never listed as at risk
  (they only appear in `unscheduled_tasks`); a deadline beyond the horizon is not a risk within it.
* `buffer_min` = free working minutes left in the task's region minus the work still required (negative = shortfall);
  `available_min = required_min + buffer_min` for placed tasks.
* Unscheduled `reason_code`s: `invalid_constraints`, `outside_horizon`, `deadline_exceeded`,
  `insufficient_time_before_deadline`, `no_available_slot`, `non_splittable_no_slot`, `min_chunk_unsatisfiable`,
  `exceeds_daily_workload`, `no_feasible_window`, `blocked_by_prerequisite`, `prerequisite_cancelled`,
  `unknown_prerequisite`, `circular_dependency`, `blocked_by_cycle`. Each comes with a specific sentence and a `suggested_action`.

## 12. Validation (`SchedulingValidator`)

Re-derives every session from the result's timeline (date + start/end) and checks against the *input* (tasks,
events, constraints) — not against engine state. Codes: `invalid_entry`, `outside_horizon`, `outside_availability`,
`task_overlap`, `break_violation`, `event_overlap`, `event_modified`, `early_start`, `late_completion`,
`deadline_violation`, `dependency_order`, `prerequisite_unscheduled`, `task_split`, `chunk_too_short`,
`duration_exceeded`, `workload_exceeded`, `terminal_task_scheduled`, `cyclic_task_scheduled`, `unknown_task`.
The engine runs it and stores findings in `validation_errors`; any finding forces `status: "infeasible"`.
`SchedulingService` then refuses to return the schedule (`500 internal_error`, details logged to stderr),
so an invalid schedule can never reach a client as a success. (`break_violation` is checked for sessions on the same day.)

## 13. Result format and statistics

`SchedulingResult::toApiJson()` = every Phase 1 key (`toJson()` is untouched) plus `start_date`, `end_date`,
`objective`, `statistics`, `validation_errors`, `warnings`; timeline rows add `date`, `session_index`,
`session_count` (null for events); slots add `date`; unscheduled rows add `reason_code`; at-risk rows add `category`,
`deadline`, `estimated_completion`. Timeline: sorted by date, start, end, events before tasks, then id; ids `tl-1…`.
Status: `success` (nothing left unplaced; at-risk tasks and event conflicts do not change it), `partial` (something
placed, something not), `infeasible` (tasks to place but none placed, or validation failed).

Statistics definitions:
* `total_window_min`: window minutes on working days in the horizon.
* `fixed_event_window_min`: event minutes inside windows (overlaps counted once).
* `total_available_min` = window − event minutes = the capacity for tasks (before breaks / workload caps). Also `total_available_min` at top level.
* `total_scheduled_min` (= top-level `total_allocated_min`): task minutes placed; each session counted once; **events are never counted**.
* `total_occupied_min` = scheduled task minutes + fixed-event window minutes.
* `utilization_pct` = `total_scheduled_min / total_available_min × 100`, two decimals (0 when no capacity).
* `tasks_scheduled`/`_partially_scheduled`/`_unscheduled` partition `tasks_considered` (active tasks).
  `total_requested_min − total_scheduled_min = total_remaining_min`.

## 14. Preview vs persistence

`optimize` never writes. A schedule-apply endpoint was **not** implemented: tasks and events have no field for a
placed time, so persisting a result would need a new table (a migration), which Phase 4 excludes, and the only
alternative (turning sessions into events) would duplicate the task data. The proposal is returned for review;
the client can create events from accepted sessions through the existing event API.

## 15. Complexity

n tasks, V/E graph size, m events, D days, W windows per day, S placed sessions, N tree size (m + S).
* Context: graph build O(n + E); Tarjan O(V + E); event insertion O(m log m + conflicts).
* Queue: O(n log n) in total (push/pop O(log n)).
* Placement of one task: O(D·W·(log N + k + k log k)) worst case with k intervals overlapping a window; stops at the
  first complete placement, so typical cost is a few windows. Whole run worst case O(n·D·W·(log N + k log k)).
* Diagnosis/analysis: one extra scan only for tasks that fail or have a deadline.
* Validator: O(S log S + S·m) (session × event check). Space O(n + E + m + S + D·W).
* Measured: 300 tasks over a 30-day horizon with daily events place and validate in well under a second (test `EnginePerformance`).

## 16. Known limitations

* **Greedy, not optimal.** Tasks are placed one at a time in objective order, each at its earliest fit; there is no
  backtracking or look-ahead. A high-ranked task can occupy time a later task with a tighter deadline needed. The
  `deadline`/`edf` objectives reduce this; `tasks_at_risk` reports what happened.
* `earliest_start_min`/`latest_completion_min` are daily time-of-day limits (the model stores minutes of a day, not dates).
* Events never move (flexible events included); `min_break_min` pads task sessions only.
* Status vocabulary is an assumption (§6). Tasks/events cannot cross midnight (model rule); no time zones or DST.
* Horizon ≤ 366 days; windows ≤ 12; `max_workload_min` is per calendar day.
* Partial placements are committed (the placed part stays on the timeline) and block dependents.
* Cycles in the request give `409`; the engine alone reports them per task.
* Tasks without an id (request mode) receive the next numeric id, like the repositories.
* Windows are applied identically to every working weekday (no per-weekday windows).

## 17. Future optimisation

Bounded backtracking / repair of deadline misses, per-weekday windows, flexible-event rescheduling, an apply endpoint with a
`scheduled_sessions` table, slack-based dynamic priority, energy/focus-aware windows, incremental re-scheduling.

## Tests

`test_scheduling_engine.cpp` (80: basic, events, objectives, dependencies, deadlines, splitting, multi-day, statistics, results,
randomised property test over 750 scenarios, performance), `test_scheduling_validator.cpp` (18: every violation code),
`test_scheduling_api.cpp` (18: real Crow route + temporary SQLite, preview leaves the database untouched), plus cycle regression
tests in `test_dependency_graph.cpp` and `test_scheduling_data_provider.cpp`.
