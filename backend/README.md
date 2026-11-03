# ChronoPlan Backend

C++20 backend for ChronoPlan, an intelligent scheduling and time-management system.
**Phase 2** added persistent storage: SQLite, a repository layer, and full Task / Event /
Dependency REST APIs. **Phase 3** added the custom data structures: an AVL interval tree, a
binary-heap priority queue, a dependency graph with topological sorting, and a
`SchedulingDataProvider`. **Phase 4** (this version) adds the deterministic **scheduling engine**:
`POST /api/v1/schedule/optimize` generates a conflict-free, dependency- and deadline-aware
single- or multi-day schedule (a read-only preview). Details: [`PHASE3_DESIGN.md`](PHASE3_DESIGN.md),
[`PHASE4_DESIGN.md`](PHASE4_DESIGN.md).

## Architecture

```
HTTP (Crow routes)  ->  repositories  ->  SQLite (C API, RAII)
src/api/                include/repositories/    include/database/
                        src/repositories/        src/database/
models (Task, Event, Schedule, Validation): header-only, no SQL, no Crow

Phase 3 (no REST endpoints; consumed by the future scheduling engine):
repositories -> services::SchedulingDataProvider -+-> IntervalTree     (occupied event time)
                                                  +-> SchedulingQueue  (ranked ready tasks, PriorityQueue)
                                                  +-> DependencyGraph  (prerequisite -> dependent)
algorithms/ depend only on the standard library + models/Validation.hpp (no Crow, no SQLite)

Phase 4:
Crow route -> services::SchedulingService -> scheduler::SchedulingEngine (no Crow, no SQLite)
                                               DependencyGraph + ObjectiveQueue (PriorityQueue) + IntervalTree
                                               -> TaskPlacer -> ScheduleBuilder/DeadlineAnalyzer -> SchedulingValidator
```

| Layer | Files | Responsibility |
|---|---|---|
| Models | `include/models/*.hpp` | Data, validation, JSON. Unchanged from Phase 1. |
| Database | `include/database/`, `src/database/` | `Database` (connection, PRAGMAs, mutex), `SQLiteStatement` (prepared statements), `SQLiteTransaction` (RAII; savepoints when nested), `Migrations`, `DatabaseManager` (startup). |
| Repositories | `include/repositories/`, `src/repositories/` | `TaskRepository`, `EventRepository`, `DependencyRepository`; bundled in `Repositories`. All SQL lives here. |
| API | `src/api/` | Routes only parse/serialise and map exceptions to JSON errors (`RouteHelpers`). No SQL. |
| Schema | `db/migrations/001_initial.sql` | Embedded into the binary at build time (CMake generates a header). |

Key decisions (Phases 2-3):

* **Type-safe ids.** Ids are stored as `(id_type, id_value)` with `id_type` in `number|string`; numeric `7` and string `"7"` are different rows and different foreign-key targets. See `API_CONTRACT.md` §1.1 for the URL form (`/tasks/7` vs `/tasks/%227%22`).
* **Dependencies are relational.** `task_dependencies` is the only place prerequisites are stored; `Task.prerequisite_ids` is derived from it on read. Foreign keys use `ON DELETE CASCADE`, so deleting a task removes every edge that mentions it.
* **Cycle detection** is an iterative DFS over the stored graph, run inside the write transaction that inserts the edge.
* **PUT = full replacement** (absent optional fields become `null`/defaults).
* **One connection + recursive mutex.** Crow runs handlers on several threads; a single serialized connection keeps transactions from interleaving. SQLite is also opened with WAL (`synchronous=NORMAL`) so other processes can read while this one writes, and `busy_timeout` (default 5 s). WAL is not used for `:memory:`. Outermost transactions are `BEGIN IMMEDIATE`; nested ones are savepoints.
* **Schema versioning.** `PRAGMA user_version` + an ordered migration list (`src/database/Migrations.cpp`). Startup applies missing migrations atomically and refuses a database newer than the binary. To add a migration: create `db/migrations/002_*.sql`, register it in `CMakeLists.txt` (generated header) and `Migrations.cpp`.
* **Startup is fail-fast.** If the database cannot be opened, migrated or verified, the process prints a generic error and exits with code 2 instead of serving requests.
* **Errors never leak SQL.** The real SQLite message is logged to stderr; clients get `internal_error` / `database_unavailable`.

## Requirements

* CMake >= 3.20, a C++20 compiler (GCC 11+, Clang 14+, MSVC 2022).
* Crow, nlohmann-json, SQLite3, GoogleTest (declared in `vcpkg.json`).

## Build, test, run

### Windows (vcpkg manifest mode, PowerShell)

```powershell
git clone https://github.com/microsoft/vcpkg $env:USERPROFILE\vcpkg
& $env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"

cd backend
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\chronoplan.exe
```
(or run `scripts\build_and_test.ps1`).

### Linux / macOS

```bash
export VCPKG_ROOT=~/vcpkg            # or install crow, nlohmann-json, sqlite3, gtest system-wide
cd backend
scripts/build_and_test.sh            # configure + build + ctest into ./build
./build/chronoplan
```
Without vcpkg, install Crow (with Asio), nlohmann_json, SQLite3 and GoogleTest anywhere CMake can find them
(`-DCMAKE_PREFIX_PATH=<prefix>`); that is how the Linux verification in PHASE4_DESIGN was run.
Equivalent manual commands:
```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Test executables (`chronoplan_*_tests`) can also be run individually. Every test uses its own temporary database file under the OS temp directory and deletes it afterwards; tests never touch the real database. Pass `-DCHRONOPLAN_BUILD_TESTS=OFF` to skip them.

## Configuration

| Variable | Default | Meaning |
|---|---|---|
| `CHRONOPLAN_DB_PATH` | `./data/chronoplan.db` | SQLite file (relative to the working directory). The parent directory is created on startup. |
| `CHRONOPLAN_DB_BUSY_TIMEOUT_MS` | `5000` | Wait time on a locked database (0..600000). |
| `CHRONOPLAN_BIND_ADDR` | `127.0.0.1` | Interface to listen on. Use `0.0.0.0` in containers / cloud hosts. |
| `PORT` | `8080` | Standard PaaS port variable. |
| `CHRONOPLAN_PORT` | – | Same as `PORT`; wins if both are set. Must be an integer 1-65535, otherwise startup aborts (exit code 3). |
| `CHRONOPLAN_CORS_ORIGIN` | `*` | `Access-Control-Allow-Origin` value (a single origin). |

See `.env.example`. The `data/` directory and `*.db*` files are git-ignored. Back up the database by copying the `.db` file while the server is stopped (in WAL mode, `-wal`/`-shm` files may exist next to it while it runs).

## Deployment notes

* **Local development (default):** nothing to set; listens on `127.0.0.1:8080` with CORS `*`.
* **Cloud / container:** `CHRONOPLAN_BIND_ADDR=0.0.0.0`, `PORT` provided by the platform, `CHRONOPLAN_CORS_ORIGIN=https://your-frontend.example.com`, and `CHRONOPLAN_DB_PATH` pointing at a **persistent volume** (container filesystems are usually ephemeral, which would lose the database on restart).
* **CORS `*` is for development only.** It lets any website call the API from a user's browser. The server prints a warning when it listens on a non-loopback address with `*`. Only one origin can be configured; there is no authentication in this phase, so do not expose the API to the public internet as-is.
* `scripts/smoke_test.sh [binary]` (Linux/macOS) starts the real binary and checks PORT, `0.0.0.0` binding, the CORS header, persistence across a restart and fail-fast on a bad `PORT`.

## Example requests

```bash
curl -X POST localhost:8080/api/v1/tasks -H 'Content-Type: application/json' \
     -d '{"title":"Write spec","duration_min":90,"priority":4}'
curl -X POST localhost:8080/api/v1/tasks -H 'Content-Type: application/json' \
     -d '{"title":"Review spec","duration_min":30,"prerequisite_ids":[1]}'
curl localhost:8080/api/v1/tasks
curl -X POST localhost:8080/api/v1/tasks/2/dependencies -H 'Content-Type: application/json' -d '{"prerequisite_id":1}'
curl -X POST localhost:8080/api/v1/events -H 'Content-Type: application/json' \
     -d '{"title":"Standup","date":"2026-11-02","start_min":540,"end_min":570}'
curl 'localhost:8080/api/v1/events?date=2026-11-02'
curl -X DELETE localhost:8080/api/v1/tasks/2
```
Full reference: [`API_CONTRACT.md`](API_CONTRACT.md).

## Phase 4: scheduling engine

`POST /api/v1/schedule/optimize` — body = the Phase 1 request (`config`, `fixed_events`, `flexible_tasks`) plus optional
Phase 4 fields. It only **reads**; nothing in the database changes.

```bash
curl -X POST localhost:8080/api/v1/schedule/optimize -H 'Content-Type: application/json' \
     -d @examples/optimize_request.json
```

**Configuring scheduling** (`config`):

| Field | Purpose |
|---|---|
| `target_date`, `end_date` | the horizon (single day when `end_date` is omitted; at most 366 days) |
| `availability_windows` | e.g. `[{"start_min":540,"end_min":720},{"start_min":780,"end_min":1080}]` = 09:00-12:00 and 13:00-18:00; the gap is never used. Without it, `work_window_start_min/_end_min` (default 08:30-18:30) is the single window |
| `working_weekdays` | e.g. `["mon","tue","wed","thu","fri"]`; default: every day |
| `min_break_min`, `max_workload_min`, `allow_splitting` | gap between task sessions, task minutes per day, global splitting switch |
| `objective` | `balanced` \| `priority` \| `deadline` \| `compact` \| `edf` |

Tasks and events can come from the body (default) or from the database: add `"data_source": "database"` and omit
`flexible_tasks` / `fixed_events` to schedule everything stored (events are taken from the horizon only).

**Reading the result.** `status` is `success` (everything placed), `partial` or `infeasible`. `timeline` is chronological and
contains the fixed events (never moved) and the task sessions; a split task has several rows with the same `reference_id`
(`session_index` / `session_count`). `unscheduled_tasks[]` always carry a `reason` and a machine-readable `reason_code`;
`tasks_at_risk[]` separates `deadline_risk`, `hard_infeasible`, `insufficient_capacity` and `dependency_blocked`; `conflicts`
lists overlapping events; `statistics` has requested/scheduled/remaining minutes, available minutes and `utilization_pct`
(scheduled task minutes / available minutes). `validation_errors` is always empty in a returned schedule: the result is
checked by an independent validator and an invalid one is never returned. Errors: `400` invalid input, `409 circular_dependency`
for dependency cycles. Full field reference: [`API_CONTRACT.md`](API_CONTRACT.md); algorithm, complexity and limitations:
[`PHASE4_DESIGN.md`](PHASE4_DESIGN.md). The algorithm is a deterministic greedy list scheduler, **not** a globally optimal solver.

**Tests.** `ctest --test-dir build --output-on-failure` runs everything. Phase 4: `chronoplan_scheduling_engine_tests`,
`chronoplan_scheduling_validator_tests`, `chronoplan_scheduling_api_tests` (real Crow route, temporary SQLite file).

## Phase 3: data structures

| Component | Files | Notes |
|---|---|---|
| `TimeInterval`, `TimeSlot` | `include/algorithms/TimeInterval.hpp` | Half-open `[start, end)` in minutes (same unit as `Event.start_min/end_min`). |
| `IntervalTree` | `include/algorithms/IntervalTree.hpp`, `src/algorithms/IntervalTree.cpp` | AVL tree augmented with subtree `maxEnd`; overlap queries and `computeFreeSlots`. |
| `PriorityQueue<T,Compare,KeyOf>` | `include/algorithms/PriorityQueue.hpp` | Header-only indexed binary heap (push/pop/update/remove by key). |
| `SchedulingCandidate` + comparator | `include/algorithms/SchedulingPriority.hpp` | Ranking only; no placement logic. |
| `DependencyGraph` | `include/algorithms/DependencyGraph.hpp`, `src/algorithms/DependencyGraph.cpp` | Adjacency lists, cycle detection, Kahn topological sort, ready tasks. |
| `SchedulingDataProvider` | `include/services/SchedulingDataProvider.hpp`, `src/services/SchedulingDataProvider.cpp` | Loads via `Repositories`; builds the three structures. |

Phase 4 note: `readyTasksExcludingCycles()` / `cyclicComponents()` were added so a task on (or downstream of) a dependency
cycle is never queued, even if its prerequisites are marked completed; `SchedulingDataProvider::buildPriorityQueue` uses them.

Usage sketch:

```cpp
repositories::Repositories repos(db);
services::SchedulingDataProvider provider(repos);
services::SchedulingDataOptions o; o.date = "2026-11-02";
auto data = provider.prepareSchedulingData(o);
data.intervals.findFreeSlots(540, 1020);   // free time 09:00-17:00
data.queue.pop();                          // highest-ranked ready task
data.graph.topologicalOrder();             // throws CycleError on cyclic data
```

Tests: `tests/test_interval_tree.cpp`, `test_priority_queue.cpp`, `test_dependency_graph.cpp`
(no database) and `test_scheduling_data_provider.cpp` (temporary SQLite file). All run under `ctest`.
