# ChronoPlan Phase 3 — Custom Data Structures & Algorithms

Scope: the DSA foundation only. No scheduling, optimisation, task splitting or timetable
generation (Phase 4). No new REST endpoints, no schema changes.

```
SQLite -> Repositories -> services::SchedulingDataProvider -+-> IntervalTree   (events -> occupied time)
                                                            +-> SchedulingQueue (ready tasks, ranked)
                                                            +-> DependencyGraph (prerequisite -> dependent)
                                                                      |
                                                                      v
                                                          Phase 4 scheduling engine
```
`include/algorithms` + `src/algorithms` depend only on the standard library and
`models/Validation.hpp` (for `Id`, `ValidationError`). They know nothing of Crow or SQLite.
Only the provider touches repositories.

## Conventions

* **Time**: integer minutes, the unit the models already use (`Event.start_min/end_min`,
  `Task.earliest_start_min`, 0..1440). Dates stay `YYYY-MM-DD` strings; an interval tree
  covers one day (the provider fills it with one date's events).
* **Half-open intervals** `[start, end)`: `[9:00,10:00)` and `[10:00,11:00)` do not overlap;
  `[9:00,11:00)` and `[10:00,12:00)` do. Overlap iff `a.start < b.end && b.start < a.end`.
* **Ids** are `models::Id`; numeric `7` and string `"7"` are different everywhere. Deterministic
  order (`IdOrdering.hpp`): numeric before string; numeric by value (2 < 10); strings lexicographic.
* **Task priority** is 1..5 and **5 is the most important** (checked in `Task.hpp`).

## 1. Interval tree (`IntervalTree`)

Why: conflict checks and free-slot search over fixed events without scanning every event.

* Node: `interval`, `left`, `right` (`unique_ptr`), `height`, `maxEnd` (max `end` in the subtree).
* Key: `(start, end, type, id)` — a strict total order, so ties are deterministic.
* Balance: AVL (single/double rotations in `rebalance`). Both `height` and `maxEnd` are recomputed
  bottom-up (`update`) after every insert, delete and rotation.
* Duplicates: an interval equal in all four key fields is rejected (`insert` returns `false`).
  Same bounds with a different id or type are distinct entries. `remove` matches the full key.
* Delete: leaf/one child splice; two children → replace with in-order successor (`removeMin`).
* `findAnyOverlap`: walk down; go left iff `left->maxEnd > start`, else right (CLRS argument:
  if the left subtree reaches past `start` but nothing there overlaps, nothing on the right can).
* `findAllOverlaps`: in-order traversal pruned by `maxEnd <= start` (skip subtree) and
  `node.start >= end` (skip node + right subtree). Results come out sorted by start.
* Free slots: `computeFreeSlots(occupied, windowStart, windowEnd, minLength)` clips intervals to the
  window, sorts, sweeps with a cursor. Input may be unsorted/overlapping/duplicated; zero-length gaps
  never appear; output is chronological. `IntervalTree::findFreeSlots` feeds it the window's overlaps.
* `checkInvariants()` verifies BST order, AVL balance, heights, `maxEnd`, size (used by tests).

| Operation | Complexity |
|---|---|
| insert / remove / contains | O(log n) |
| findAnyOverlap | O(log n) |
| findAllOverlaps | O(log n + k) typical, **O(n) worst case** (not guaranteed output-sensitive) |
| findFreeSlots | overlap query + O(k log k) |
| computeFreeSlots (vector) | O(m log m) |
| space | O(n) |

## 2. Priority queue (`PriorityQueue`, `SchedulingPriority.hpp`)

* Vector-backed binary heap, `siftUp`/`siftDown`, plus an `unordered_map<key, position>` so
  `update` and `remove` by key are O(log n). The heap itself is custom; `std::priority_queue`
  is not used.
* `Compare(a,b) == true` ⇒ `a` is extracted before `b` (`std::less` = min-heap).
* Duplicate policy: `push` with an existing key returns `false` and changes nothing; `assign`
  throws `std::invalid_argument`. `pop()` on an empty queue returns `std::nullopt`, `top()` throws,
  `peek()` returns `nullptr`.
* Scheduling ranking (`compareCandidates`, first difference wins): higher priority → has a deadline
  before none, earlier `deadline_date` then `deadline_min` → earlier `earliest_start_min`
  (absent = 0) → longer `duration_min` → smaller task id. It is a total order, so output is
  deterministic. This only *ranks*; it does not place anything.

| Operation | Complexity |
|---|---|
| push, pop, update, remove | O(log n) |
| peek / top / contains / find | O(1) (average for the hash lookups) |
| assign (bottom-up build) | O(n) |

## 3. Dependency graph (`DependencyGraph`)

* Adjacency lists in an `unordered_map<key, Vertex{id, out, in}>`; both directions are stored.
* **Edge `A -> B` means A must be completed before B.** The Phase 2 `Dependency{task, prerequisite}`
  becomes `addEdge(prerequisite, task)`.
* Rejected: self-edges (`SelfEdgeError`), duplicates (`DuplicateEdgeError`), missing vertices
  (`MissingVertexError`), empty ids (`GraphError`). Cycles are *not* rejected on insert (stored data
  may contain them); use `wouldCreateCycle` before inserting or `findCycle` afterwards.
* Cycle detection: iterative three-colour DFS (no recursion, so very long chains are safe). Returns an
  actual cycle witness. Independent of the Phase 2 SQL check.
* Topological sort: Kahn's algorithm. Eligible vertices sit in the custom `PriorityQueue` keyed by
  id order, so the smallest eligible id goes first (deterministic, independent of insertion order).
  On a cycle `topologicalSort()` returns `success=false` with the sortable prefix and the
  `unresolved` vertices; `topologicalOrder()` throws `CycleError`.
* `readyTasks(completed)`: vertices not in `completed` whose prerequisites are all in `completed`
  (isolated vertices included; unknown completed ids ignored), sorted by id.

| Operation | Complexity |
|---|---|
| addVertex | O(1) average |
| addEdge / removeEdge / hasEdge | O(d), d = degree (duplicate scan) |
| removeVertex | O(d) plus O(d) per neighbour list |
| getPrerequisites / getDependents | O(d) |
| wouldCreateCycle | O(V + E) |
| hasCycle / findCycle | O(V + E) (+ O(V log V) for deterministic root order) |
| topologicalSort | O((V + E) log V) |
| readyTasks | O(V + E + C + R log R) |
| space | O(V + E) |

## 4. `SchedulingDataProvider`

`prepareSchedulingData(options)` performs one query per repository (`getAllTasks`,
`getEventsByDate(date)`, `getAllDependencies`) and then builds, in memory:

* `graph` — one vertex per task, one edge per stored dependency;
* `cycle` — a cycle witness if the stored data contains one;
* `intervals` — one `Event` interval per event on `options.date` (`include_flexible_events`, default
  `true`, because flexible events still occupy time until Phase 4 moves them);
* `queue` — tasks that are *ready* (every prerequisite in `completed_task_ids`) and not completed.
  Blocked tasks and tasks on a cycle are not queued; the backend does not know the frontend's status
  vocabulary, so completion is supplied by the caller;
* `warnings` — anything skipped (unknown dependency endpoint, duplicate interval, …).

The static builders (`buildDependencyGraph`, `buildIntervalIndex`, `buildPriorityQueue`,
`toCandidate`) are pure and take plain vectors. A bad date surfaces as `models::ValidationError`.

Phase 4 consumes `SchedulingData`: free slots from `intervals`, the next task from `queue`, ordering
and readiness from `graph`; as it schedules a task it can insert a `ScheduledTask` interval into the
tree and re-run `readyTasks`. None of that logic exists yet.

## Tests

`test_interval_tree.cpp`, `test_priority_queue.cpp`, `test_dependency_graph.cpp` — structural
invariants after operations, randomised comparison against brute-force/reference models, large
inputs. `test_scheduling_data_provider.cpp` — real repositories on a temporary SQLite file.
