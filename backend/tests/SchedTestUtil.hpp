#pragma once
// Helpers shared by the Phase 4 scheduling tests (engine, validator, API).
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "models/Schedule.hpp"
#include "scheduler/CalendarDate.hpp"
#include "scheduler/SchedulingContext.hpp"
#include "scheduler/SchedulingEngine.hpp"
#include "scheduler/SchedulingValidator.hpp"

namespace schedtest {

using nlohmann::json;
using namespace chronoplan;
using models::SchedulingResult;

constexpr int H(int h, int m = 0) { return h * 60 + m; }
inline const char* kMon = "2026-11-02";   // a Monday
// Date `n` days after kMon.
inline std::string formatDay(int n) { return chronoplan::scheduler::formatDayNumber(chronoplan::scheduler::dayNumberOf(kMon) + n); }

// Config: Monday 2026-11-02, window 09:00-17:00, no breaks, no workload cap. `over` overrides keys.
inline json cfg(const json& over = json::object()) {
    json c = {{"target_date", kMon}, {"work_window_start_min", H(9)}, {"work_window_end_min", H(17)},
              {"min_break_min", 0}, {"max_workload_min", 1440}, {"objective", "priority"}};
    for (auto it = over.begin(); it != over.end(); ++it) c[it.key()] = it.value();
    return c;
}

// Task JSON: id, duration, extras. Title = "T<id>". Splitting defaults to OFF to keep tests
// simple; tests that want splitting say {"allow_split", true}.
inline json task(const json& id, int duration, const json& extra = json::object()) {
    json t = {{"id", id}, {"title", "T" + (id.is_string() ? id.get<std::string>() : id.dump())},
              {"duration_min", duration}, {"allow_split", false}};
    for (auto it = extra.begin(); it != extra.end(); ++it) t[it.key()] = it.value();
    return t;
}

inline json event(const json& id, int start, int end, const std::string& date = kMon,
                  const json& extra = json::object()) {
    json e = {{"id", id}, {"title", "E" + (id.is_string() ? id.get<std::string>() : id.dump())},
              {"date", date}, {"start_min", start}, {"end_min", end}};
    for (auto it = extra.begin(); it != extra.end(); ++it) e[it.key()] = it.value();
    return e;
}

// nlohmann turns a one-element brace list `{obj}` into the object itself, so wrap it back.
inline json asArray(json v) { return v.is_array() ? v : (v.is_null() ? json::array() : json::array({v})); }
inline json body(const json& config, const json& tasks = json::array(), const json& events = json::array()) {
    return {{"config", config}, {"flexible_tasks", asArray(tasks)}, {"fixed_events", asArray(events)}};
}

inline SchedulingResult run(const json& b) { return scheduler::SchedulingEngine::run(models::SchedulingRequest::fromJson(b)); }

struct Sess { std::string date; int start; int end; };
inline bool sameId(const models::Id& a, const json& j) { return a.toJson() == j; }

inline std::vector<Sess> sessionsOf(const SchedulingResult& r, const json& id) {
    std::vector<Sess> v;
    for (const auto& e : r.timeline)
        if (e.type == models::TimelineType::Task && sameId(e.reference_id, id)) v.push_back({e.date, e.start_min, e.end_min});
    return v;
}
inline int scheduledMinutes(const SchedulingResult& r, const json& id) {
    int m = 0;
    for (const auto& s : sessionsOf(r, id)) m += s.end - s.start;
    return m;
}
inline const models::UnscheduledTask* unscheduled(const SchedulingResult& r, const json& id) {
    for (const auto& u : r.unscheduled_tasks) if (sameId(u.task_id, id)) return &u;
    return nullptr;
}
inline const models::TaskAtRisk* atRisk(const SchedulingResult& r, const json& id) {
    for (const auto& u : r.tasks_at_risk) if (sameId(u.task_id, id)) return &u;
    return nullptr;
}
// Task ids in order of their first session on the timeline.
inline std::vector<std::string> taskOrder(const SchedulingResult& r) {
    std::vector<std::string> out;
    for (const auto& e : r.timeline) {
        if (e.type != models::TimelineType::Task) continue;
        const std::string k = e.reference_id.value;
        if (std::find(out.begin(), out.end(), k) == out.end()) out.push_back(k);
    }
    return out;
}
inline int completionOf(const SchedulingResult& r, const json& id) {   // minute of day (single-day tests)
    int m = -1;
    for (const auto& s : sessionsOf(r, id)) m = (std::max)(m, s.end);
    return m;
}

// Every schedule the engine returns must pass the independent validator (the engine runs it
// itself and stores the findings in validation_errors).
#define EXPECT_VALID(result) \
    EXPECT_TRUE((result).validation_errors.empty()) << (result).toApiJson()["validation_errors"].dump(2)

}  // namespace schedtest
