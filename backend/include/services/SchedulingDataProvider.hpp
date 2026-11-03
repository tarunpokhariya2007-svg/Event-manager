#pragma once
// SchedulingDataProvider: loads tasks/events/dependencies through the Phase 2 repositories
// and builds the Phase 3 structures. It prepares data only; it never decides WHERE a task
// goes (that is the Phase 4 scheduling engine).
#include <string>
#include <vector>
#include "algorithms/DependencyGraph.hpp"
#include "algorithms/IntervalTree.hpp"
#include "algorithms/SchedulingPriority.hpp"
#include "models/Event.hpp"
#include "models/Task.hpp"
#include "repositories/Repositories.hpp"

namespace chronoplan::services {

struct SchedulingDataOptions {
    std::string date;                              // YYYY-MM-DD whose events fill the interval index
    std::vector<models::Id> completed_task_ids;    // treated as finished (never queued; unlock dependents)
    bool include_flexible_events = true;           // flexible events still occupy time until Phase 4 moves them
};

struct SchedulingData {
    std::string date;
    std::vector<models::Task> tasks;               // every stored task
    std::vector<models::Event> events;             // events on `date`
    algorithms::DependencyGraph graph;             // prerequisite -> dependent, one vertex per task
    algorithms::IntervalTree intervals;            // occupied [start_min, end_min) of the events
    algorithms::SchedulingQueue queue;             // ready, not-completed tasks, ranked
    std::optional<std::vector<models::Id>> cycle;  // a dependency cycle, if the stored data has one
    std::vector<std::string> warnings;             // skipped/ignored data (never silently dropped)
};

class SchedulingDataProvider {
public:
    explicit SchedulingDataProvider(repositories::Repositories& repos) : repos_(repos) {}

    // Repository access (one query each; no queries happen inside algorithm loops).
    std::vector<models::Task> loadTasks();
    std::vector<models::Event> loadEvents(const std::string& date);   // ValidationError on a bad date
    std::vector<repositories::Dependency> loadDependencies();
    // Phase 4: every stored event in ONE query (the optimizer filters to its horizon in memory,
    // instead of one getEventsByDate query per day).
    std::vector<models::Event> loadAllEvents();

    // Pure builders (no database access).
    static algorithms::DependencyGraph buildDependencyGraph(const std::vector<models::Task>& tasks,
                                                            const std::vector<repositories::Dependency>& deps,
                                                            std::vector<std::string>& warnings);
    static algorithms::IntervalTree buildIntervalIndex(const std::vector<models::Event>& events,
                                                       bool includeFlexible,
                                                       std::vector<std::string>& warnings);
    static algorithms::SchedulingCandidate toCandidate(const models::Task& task);
    // Queues tasks that are ready (all prerequisites in `completed`) and not completed themselves.
    // Tasks on or downstream of a dependency cycle are never queued (Phase 4 fix).
    static algorithms::SchedulingQueue buildPriorityQueue(const std::vector<models::Task>& tasks,
                                                          const algorithms::DependencyGraph& graph,
                                                          const std::vector<models::Id>& completed,
                                                          std::vector<std::string>& warnings);

    SchedulingData prepareSchedulingData(const SchedulingDataOptions& options);

private:
    repositories::Repositories& repos_;
};

}  // namespace chronoplan::services
