#include "services/SchedulingDataProvider.hpp"

#include <unordered_map>

namespace chronoplan::services {

using namespace chronoplan::algorithms;

namespace {
std::string describe(const models::Id& id) { return id.numeric ? id.value : "\"" + id.value + "\""; }
}  // namespace

std::vector<models::Task> SchedulingDataProvider::loadTasks() { return repos_.tasks.getAllTasks(); }

std::vector<models::Event> SchedulingDataProvider::loadEvents(const std::string& date) {
    return repos_.events.getEventsByDate(date);
}

std::vector<models::Event> SchedulingDataProvider::loadAllEvents() { return repos_.events.getAllEvents(); }

std::vector<repositories::Dependency> SchedulingDataProvider::loadDependencies() {
    return repos_.dependencies.getAllDependencies();
}

DependencyGraph SchedulingDataProvider::buildDependencyGraph(const std::vector<models::Task>& tasks,
                                                             const std::vector<repositories::Dependency>& deps,
                                                             std::vector<std::string>& warnings) {
    DependencyGraph graph;
    for (const models::Task& t : tasks) {
        try {
            graph.addVertex(t.id);
        } catch (const GraphError& e) {
            warnings.push_back(std::string("task skipped: ") + e.what());
        }
    }
    for (const repositories::Dependency& d : deps) {
        try {
            graph.addEdge(d.prerequisite, d.task);      // "task requires prerequisite" => prerequisite -> task
        } catch (const GraphError& e) {
            warnings.push_back("dependency " + describe(d.prerequisite) + " -> " + describe(d.task) +
                               " ignored: " + e.what());
        }
    }
    return graph;
}

IntervalTree SchedulingDataProvider::buildIntervalIndex(const std::vector<models::Event>& events,
                                                        bool includeFlexible,
                                                        std::vector<std::string>& warnings) {
    IntervalTree tree;
    for (const models::Event& e : events) {
        if (e.isFlexible() && !includeFlexible) continue;
        try {
            if (!tree.insert(TimeInterval(e.start_min, e.end_min, e.id, IntervalType::Event)))
                warnings.push_back("duplicate interval for event " + describe(e.id) + " ignored");
        } catch (const models::ValidationError& ex) {
            warnings.push_back("event " + describe(e.id) + " skipped: " + ex.what());
        }
    }
    return tree;
}

SchedulingCandidate SchedulingDataProvider::toCandidate(const models::Task& t) {
    SchedulingCandidate c;
    c.task_id = t.id;
    c.priority = t.priority;
    c.deadline_date = t.deadline_date;
    c.deadline_min = t.deadline_min;
    c.earliest_start_min = t.earliest_start_min.value_or(0);
    c.duration_min = t.duration_min;
    return c;
}

SchedulingQueue SchedulingDataProvider::buildPriorityQueue(const std::vector<models::Task>& tasks,
                                                           const DependencyGraph& graph,
                                                           const std::vector<models::Id>& completed,
                                                           std::vector<std::string>& warnings) {
    std::unordered_map<std::string, const models::Task*> byKey;
    byKey.reserve(tasks.size());
    for (const models::Task& t : tasks) byKey.emplace(t.id.key(), &t);

    std::vector<SchedulingCandidate> candidates;
    // Cycle-safe: a task on (or downstream of) a dependency cycle is never queued, even when
    // its direct prerequisites are marked completed.
    for (const models::Id& id : graph.readyTasksExcludingCycles(completed)) {
        const auto it = byKey.find(id.key());
        if (it == byKey.end()) {
            warnings.push_back("ready vertex " + describe(id) + " has no matching task; not queued");
            continue;
        }
        candidates.push_back(toCandidate(*it->second));
    }
    SchedulingQueue queue;
    queue.assign(std::move(candidates));     // O(n) heap construction; ids are unique
    return queue;
}

SchedulingData SchedulingDataProvider::prepareSchedulingData(const SchedulingDataOptions& options) {
    SchedulingData data;
    data.date = options.date;
    data.tasks = loadTasks();
    data.events = loadEvents(options.date);
    const std::vector<repositories::Dependency> deps = loadDependencies();

    data.graph = buildDependencyGraph(data.tasks, deps, data.warnings);
    data.cycle = data.graph.findCycle();
    data.intervals = buildIntervalIndex(data.events, options.include_flexible_events, data.warnings);
    data.queue = buildPriorityQueue(data.tasks, data.graph, options.completed_task_ids, data.warnings);
    return data;
}

}  // namespace chronoplan::services
