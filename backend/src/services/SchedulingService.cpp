#include "services/SchedulingService.hpp"
#include <iostream>
#include <set>
#include "repositories/RepositoryErrors.hpp"
#include "scheduler/SchedulingEngine.hpp"
#include "services/SchedulingDataProvider.hpp"

namespace chronoplan::services {

using models::Id;

namespace {
std::string describe(const Id& id) { return id.numeric ? id.value : "\"" + id.value + "\""; }
}  // namespace

models::SchedulingResult SchedulingService::optimize(const nlohmann::json& body) {
    models::requireObject(body, "request body");
    const std::string source = models::getOr<std::string>(body, "data_source", "request");
    if (source != "request" && source != "database")
        throw models::ValidationError("data_source must be 'request' or 'database'");

    models::SchedulingRequest request = models::SchedulingRequest::fromJson(body);   // validates config, tasks, events

    if (source == "database") {
        if (!request.flexible_tasks.empty() || !request.fixed_events.empty())
            throw models::ValidationError(
                "with data_source 'database', flexible_tasks and fixed_events must be omitted or empty");
        SchedulingDataProvider provider(repos_);
        request.flexible_tasks = provider.loadTasks();             // one query + prerequisites
        const std::string last = request.config.lastDate();
        for (models::Event& e : provider.loadAllEvents())          // one query, filtered in memory
            if (e.date >= request.config.target_date && e.date <= last) request.fixed_events.push_back(std::move(e));
    }

    // Reject dangling prerequisite references (the engine would only report them as unscheduled).
    {
        std::set<std::string> known;
        for (const models::Task& t : request.flexible_tasks)
            if (!t.id.empty()) known.insert(t.id.key());
        for (const models::Task& t : request.flexible_tasks)
            for (const Id& p : t.prerequisite_ids)
                if (!known.count(p.key()))
                    throw models::ValidationError("task " + (t.id.empty() ? "'" + t.title + "'" : describe(t.id)) +
                                                  " lists prerequisite " + describe(p) +
                                                  " which is not in the task list");
    }

    scheduler::SchedulingContext ctx = scheduler::SchedulingContext::build(std::move(request));

    // A dependency cycle makes the input invalid: report it as 409 circular_dependency.
    for (size_t i = 0; i < ctx.tasks.size(); ++i) {
        if (!ctx.infos[i].in_cycle) continue;
        std::string members;
        for (const Id& m : ctx.infos[i].cycle_members) members += (members.empty() ? "" : ", ") + describe(m);
        throw repositories::CircularDependencyError("tasks form a dependency cycle: " + members);
    }

    models::SchedulingResult result = scheduler::SchedulingEngine::run(ctx);
    if (!result.validation_errors.empty()) {
        for (const auto& v : result.validation_errors) std::cerr << "[scheduler] validation failed: " << v.code << ": " << v.message << '\n';
        throw std::runtime_error("generated schedule failed validation");
    }
    return result;
}

}  // namespace chronoplan::services
