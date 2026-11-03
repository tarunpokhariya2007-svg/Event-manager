#include "scheduler/SchedulingContext.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include "algorithms/IdOrdering.hpp"

namespace chronoplan::scheduler {

using algorithms::IntervalType;
using algorithms::TimeInterval;
using models::Id;

TaskState classifyStatus(const std::optional<std::string>& status) {
    if (!status) return TaskState::Active;
    std::string s;
    for (const char c : *status)
        if (c != ' ') s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s == "completed" || s == "complete" || s == "done" || s == "finished") return TaskState::Completed;
    if (s == "cancelled" || s == "canceled") return TaskState::Cancelled;
    return TaskState::Active;
}

long long SchedulingContext::absStart(const models::Event& e) const {
    return static_cast<long long>(dayIndexOf(e.date)) * kMinutesPerDay + e.start_min;
}

int SchedulingContext::dayIndexOf(const std::string& date) const {
    return static_cast<int>(dayNumberOf(date) - constraints.start_day);
}

namespace {

Id numericId(long long v) {
    Id id;
    id.value = std::to_string(v);
    id.numeric = true;
    return id;
}

template <class T>
void assignMissingIds(std::vector<T>& items) {
    long long next = 1;
    for (const T& x : items)
        if (x.id.numeric) next = (std::max)(next, std::stoll(x.id.value) + 1);
    for (T& x : items)
        if (x.id.empty()) x.id = numericId(next++);
}

std::string describe(const Id& id) { return id.numeric ? id.value : "\"" + id.value + "\""; }

}  // namespace

SchedulingContext SchedulingContext::build(models::SchedulingRequest request) {
    SchedulingContext ctx;
    ctx.constraints = SchedulingConstraints::fromConfig(request.config);
    const SchedulingConstraints& c = ctx.constraints;

    // ---- tasks
    assignMissingIds(request.flexible_tasks);
    ctx.tasks = std::move(request.flexible_tasks);
    ctx.infos.resize(ctx.tasks.size());
    for (size_t i = 0; i < ctx.tasks.size(); ++i) {
        const models::Task& t = ctx.tasks[i];
        if (!ctx.task_index.emplace(t.id.key(), i).second)
            throw models::ValidationError("duplicate task id " + describe(t.id));
        TaskInfo& info = ctx.infos[i];
        info.state = classifyStatus(t.status);
        try {
            t.validate();
        } catch (const models::ValidationError& e) {
            info.invalid = true;
            info.invalid_reason = e.what();
        }
        // Deadline policy: a deadline exists only when deadline_date is set; it is the minute
        // deadline_min of that date (default 1440 = the end of that day).
        if (!info.invalid && t.deadline_date)
            info.deadline_abs = (dayNumberOf(*t.deadline_date) - c.start_day) * kMinutesPerDay + t.deadline_min;
    }

    // ---- dependency graph (prerequisite -> dependent)
    for (const models::Task& t : ctx.tasks) ctx.graph.addVertex(t.id);
    for (size_t i = 0; i < ctx.tasks.size(); ++i) {
        for (const Id& p : ctx.tasks[i].prerequisite_ids) {
            const auto it = ctx.task_index.find(p.key());
            if (it == ctx.task_index.end()) {
                ctx.infos[i].unknown_prerequisites.push_back(p);
                continue;
            }
            try {
                ctx.graph.addEdge(p, ctx.tasks[i].id);
                ctx.infos[i].prerequisites.push_back(it->second);
            } catch (const algorithms::GraphError& e) {
                ctx.warnings.push_back("dependency of task " + describe(ctx.tasks[i].id) + " on " +
                                       describe(p) + " ignored: " + e.what());
            }
        }
    }
    // Cycle membership and everything downstream of a cycle (see DependencyGraph::cyclicComponents).
    std::vector<Id> members;
    for (const auto& comp : ctx.graph.cyclicComponents()) {
        for (const Id& id : comp) {
            TaskInfo& info = ctx.infos[ctx.task_index.at(id.key())];
            info.in_cycle = true;
            info.cycle_members = comp;
            members.push_back(id);
        }
    }
    for (const Id& id : ctx.graph.reachableFrom(members))
        ctx.infos[ctx.task_index.at(id.key())].downstream_of_cycle = true;

    // ---- fixed events
    assignMissingIds(request.fixed_events);
    std::vector<models::Event> inside;
    for (models::Event& e : request.fixed_events) {
        try {
            e.validate();
        } catch (const models::ValidationError& ex) {
            throw models::ValidationError("event " + describe(e.id) + " is invalid: " + ex.what());
        }
        const long long d = dayNumberOf(e.date) - c.start_day;
        if (d < 0 || d >= c.day_count) { ++ctx.events_outside_horizon; continue; }   // cannot affect the result
        inside.push_back(std::move(e));
    }
    std::sort(inside.begin(), inside.end(), [&](const models::Event& a, const models::Event& b) {
        const long long sa = ctx.absStart(a), sb = ctx.absStart(b);
        if (sa != sb) return sa < sb;
        if (a.end_min != b.end_min) return a.end_min < b.end_min;
        return algorithms::compareIds(a.id, b.id) < 0;
    });
    ctx.events = std::move(inside);

    // Every event (fixed AND flexible) occupies its stored interval; the engine never moves events.
    int conflictNo = 0;
    for (const models::Event& e : ctx.events) {
        const int s = static_cast<int>(ctx.absStart(e));
        const int en = s + e.duration();
        for (const TimeInterval& other : ctx.fixed_intervals.findAllOverlaps(s, en)) {
            models::Conflict cf;
            cf.id = "cf-" + std::to_string(++conflictNo);
            cf.event_a_id = other.id;
            cf.event_b_id = e.id;
            cf.date = e.date;
            cf.overlap_start_min = (std::max)(other.start, s) - ctx.dayIndexOf(e.date) * kMinutesPerDay;
            cf.overlap_end_min = (std::min)(other.end, en) - ctx.dayIndexOf(e.date) * kMinutesPerDay;
            cf.message = "Events " + describe(other.id) + " and " + describe(e.id) + " overlap on " + e.date;
            ctx.conflicts.push_back(std::move(cf));
        }
        ctx.fixed_intervals.insert(TimeInterval(s, en, e.id, IntervalType::Event));
    }
    return ctx;
}

}  // namespace chronoplan::scheduler
