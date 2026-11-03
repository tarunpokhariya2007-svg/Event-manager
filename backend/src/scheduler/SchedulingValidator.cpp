#include "scheduler/SchedulingValidator.hpp"
#include <algorithm>
#include <map>
#include <set>
#include "algorithms/IdOrdering.hpp"

namespace chronoplan::scheduler {

using models::Id;
using models::ScheduleViolation;
using models::TimelineType;

namespace {

struct Session {
    const models::TimelineEntry* entry;
    size_t task_index;
    int day;
    int start_abs;
    int end_abs;
};

std::string label(const models::Task& t) {
    return "task '" + t.title + "' (" + (t.id.numeric ? t.id.value : "\"" + t.id.value + "\"") + ")";
}

}  // namespace

std::vector<ScheduleViolation> SchedulingValidator::validate(const SchedulingContext& ctx,
                                                             const models::SchedulingResult& result) {
    std::vector<ScheduleViolation> out;
    const SchedulingConstraints& c = ctx.constraints;
    auto add = [&](const std::string& code, const std::string& msg, const Id& task = Id{},
                   const std::string& date = "") {
        ScheduleViolation v;
        v.code = code; v.message = msg; v.task_id = task; v.date = date;
        out.push_back(std::move(v));
    };

    // ---- 12/13: identifiers, dates and boundaries of every timeline entry
    std::set<std::string> rowIds;
    std::vector<Session> sessions;
    std::vector<const models::TimelineEntry*> eventRows;
    for (const models::TimelineEntry& e : result.timeline) {
        if (e.id.empty() || !rowIds.insert(e.id).second) {
            add("invalid_entry", "timeline entry has an empty or duplicate id '" + e.id + "'");
            continue;
        }
        if (e.reference_id.empty()) { add("invalid_entry", "timeline entry " + e.id + " has no reference_id"); continue; }
        if (e.start_min < 0 || e.end_min > kMinutesPerDay || e.start_min >= e.end_min) {
            add("invalid_entry", "timeline entry " + e.id + " has invalid boundaries", e.reference_id, e.date);
            continue;
        }
        if (!models::isValidDate(e.date)) {
            add("invalid_entry", "timeline entry " + e.id + " has an invalid date '" + e.date + "'", e.reference_id);
            continue;
        }
        const long long day = dayNumberOf(e.date) - c.start_day;
        if (day < 0 || day >= c.day_count) {
            add("outside_horizon", "timeline entry " + e.id + " is on " + e.date + ", outside the horizon " +
                                       c.start_date + " to " + c.end_date, e.reference_id, e.date);
            continue;
        }
        if (e.type == TimelineType::Event) { eventRows.push_back(&e); continue; }
        const auto it = ctx.task_index.find(e.reference_id.key());
        if (it == ctx.task_index.end()) {
            add("unknown_task", "timeline entry " + e.id + " refers to an unknown task", e.reference_id, e.date);
            continue;
        }
        sessions.push_back({&e, it->second, static_cast<int>(day),
                            static_cast<int>(day) * kMinutesPerDay + e.start_min,
                            static_cast<int>(day) * kMinutesPerDay + e.end_min});
    }

    // ---- fixed events must be present and unchanged (and nothing extra invented)
    {
        std::multiset<std::string> expected, actual;
        auto sig = [](const Id& id, const std::string& date, int s, int e) {
            return id.key() + "|" + date + "|" + std::to_string(s) + "|" + std::to_string(e);
        };
        for (const models::Event& ev : ctx.events) expected.insert(sig(ev.id, ev.date, ev.start_min, ev.end_min));
        for (const auto* row : eventRows) actual.insert(sig(row->reference_id, row->date, row->start_min, row->end_min));
        if (expected != actual)
            add("event_modified", "the fixed events in the timeline differ from the input events (moved, missing or extra)");
    }

    std::sort(sessions.begin(), sessions.end(), [](const Session& a, const Session& b) {
        if (a.start_abs != b.start_abs) return a.start_abs < b.start_abs;
        return a.end_abs < b.end_abs;
    });

    // ---- 1 / break: sessions must not overlap and keep min_break_min between each other
    for (size_t i = 1; i < sessions.size(); ++i) {
        const Session& a = sessions[i - 1];
        const Session& b = sessions[i];
        if (b.start_abs < a.end_abs) {
            add("task_overlap", label(ctx.task(a.task_index)) + " overlaps " + label(ctx.task(b.task_index)),
                ctx.task(b.task_index).id, b.entry->date);
        } else if (b.start_abs - a.end_abs < c.min_break_min && a.day == b.day) {
            add("break_violation", "less than " + std::to_string(c.min_break_min) + " minutes between " +
                                       label(ctx.task(a.task_index)) + " and " + label(ctx.task(b.task_index)),
                ctx.task(b.task_index).id, b.entry->date);
        }
    }
    // (sorted by start, so a long session could overlap a non-adjacent one; check the max end too)
    {
        int maxEnd = -1; size_t maxIdx = 0;
        for (size_t i = 0; i < sessions.size(); ++i) {
            if (i > 0 && sessions[i].start_abs < maxEnd && maxIdx != i - 1)
                add("task_overlap", label(ctx.task(sessions[i].task_index)) + " overlaps " +
                                        label(ctx.task(sessions[maxIdx].task_index)),
                    ctx.task(sessions[i].task_index).id, sessions[i].entry->date);
            if (sessions[i].end_abs > maxEnd) { maxEnd = sessions[i].end_abs; maxIdx = i; }
        }
    }

    // ---- 2: sessions vs the INPUT fixed events
    for (const Session& s : sessions)
        for (const models::Event& ev : ctx.events) {
            if (ctx.dayIndexOf(ev.date) != s.day) continue;
            if (s.entry->start_min < ev.end_min && ev.start_min < s.entry->end_min)
                add("event_overlap", label(ctx.task(s.task_index)) + " overlaps fixed event '" + ev.title + "'",
                    ctx.task(s.task_index).id, ev.date);
        }

    // ---- per task checks
    std::vector<std::vector<const Session*>> byTask(ctx.tasks.size());
    std::vector<int> dayLoad(static_cast<size_t>(c.day_count), 0);
    for (const Session& s : sessions) {
        byTask[s.task_index].push_back(&s);
        dayLoad[static_cast<size_t>(s.day)] += s.entry->end_min - s.entry->start_min;
    }
    for (int d = 0; d < c.day_count; ++d)
        if (dayLoad[static_cast<size_t>(d)] > c.max_workload_min)
            add("workload_exceeded", "task minutes on " + c.dateString(d) + " (" + std::to_string(dayLoad[static_cast<size_t>(d)]) +
                                         ") exceed max_workload_min " + std::to_string(c.max_workload_min), Id{}, c.dateString(d));

    auto completionOf = [&](size_t i) {
        int m = -1;
        for (const Session* s : byTask[i]) m = (std::max)(m, s->end_abs);
        return m;
    };
    auto scheduledOf = [&](size_t i) {
        int m = 0;
        for (const Session* s : byTask[i]) m += s->entry->end_min - s->entry->start_min;
        return m;
    };

    for (size_t i = 0; i < ctx.tasks.size(); ++i) {
        const auto& list = byTask[i];
        if (list.empty()) continue;
        const models::Task& t = ctx.task(i);
        const TaskInfo& info = ctx.infos[i];
        if (info.state != TaskState::Active)
            add("terminal_task_scheduled", label(t) + " is completed/cancelled and must not be scheduled", t.id);
        if (info.in_cycle || info.downstream_of_cycle)
            add("cyclic_task_scheduled", label(t) + " is on or downstream of a dependency cycle", t.id);

        const bool mayLeaveSplit = t.allow_split && c.allow_splitting;
        if (!mayLeaveSplit && list.size() > 1) add("task_split", label(t) + " is not splittable but has several sessions", t.id);
        const int total = scheduledOf(i);
        if (total > t.duration_min)
            add("duration_exceeded", label(t) + " has " + std::to_string(total) + " scheduled minutes, duration is " +
                                         std::to_string(t.duration_min), t.id);
        for (const Session* s : list) {
            const int len = s->entry->end_min - s->entry->start_min;
            if (mayLeaveSplit ? len < t.min_chunk_min : len != t.duration_min)
                add("chunk_too_short", label(t) + " has a session of " + std::to_string(len) + " minutes (" +
                                           (mayLeaveSplit ? "min_chunk_min " + std::to_string(t.min_chunk_min)
                                                          : "task is not splittable, duration " + std::to_string(t.duration_min)) + ")",
                    t.id, s->entry->date);
            // availability: whole session inside ONE window on an active weekday
            bool inWindow = false;
            if (c.isActiveDay(s->day))
                for (const auto& w : c.windows)
                    if (s->entry->start_min >= w.start_min && s->entry->end_min <= w.end_min) inWindow = true;
            if (!inWindow)
                add("outside_availability", label(t) + " is scheduled outside the availability windows on " + s->entry->date,
                    t.id, s->entry->date);
            if (t.earliest_start_min && s->entry->start_min < *t.earliest_start_min)
                add("early_start", label(t) + " starts before earliest_start_min", t.id, s->entry->date);
            if (t.latest_completion_min && s->entry->end_min > *t.latest_completion_min)
                add("late_completion", label(t) + " ends after latest_completion_min", t.id, s->entry->date);
            if (info.deadline_abs && s->end_abs > *info.deadline_abs)
                add("deadline_violation", label(t) + " ends after its deadline", t.id, s->entry->date);
        }
        // dependencies: every session starts at/after every prerequisite's completion
        const int firstStart = list.front()->start_abs;
        int first = firstStart;
        for (const Session* s : list) first = (std::min)(first, s->start_abs);
        for (const Id& p : t.prerequisite_ids) {
            const auto it = ctx.task_index.find(p.key());
            if (it == ctx.task_index.end()) {
                add("prerequisite_unscheduled", label(t) + " has an unknown prerequisite", t.id);
                continue;
            }
            const size_t pi = it->second;
            if (ctx.infos[pi].state == TaskState::Completed) continue;
            if (ctx.infos[pi].state == TaskState::Cancelled || scheduledOf(pi) < ctx.task(pi).duration_min) {
                add("prerequisite_unscheduled", label(t) + " is scheduled but prerequisite " + label(ctx.task(pi)) +
                                                    " is not fully scheduled", t.id);
                continue;
            }
            if (first < completionOf(pi))
                add("dependency_order", label(t) + " starts before prerequisite " + label(ctx.task(pi)) + " finishes",
                    t.id, list.front()->entry->date);
        }
    }
    return out;
}

}  // namespace chronoplan::scheduler
