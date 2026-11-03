#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "models/Event.hpp"
#include "models/Task.hpp"

namespace chronoplan::models {

// Accepted values for config.objective. ASSUMPTION: the frontend's real list has
// not been verified; change this one array (and scheduler/SchedulingObjective.hpp) if it differs.
// Phase 4 implements all five; "edf" is an additive value (pure earliest-deadline-first).
inline constexpr std::array<std::string_view, 5> kObjectives = {"balanced", "priority", "deadline",
                                                                "compact", "edf"};

inline bool isValidObjective(const std::string& o) {
    for (const std::string_view v : kObjectives)
        if (v == o) return true;
    return false;
}

// One daily availability window [start_min, end_min), minutes since midnight.
struct AvailabilityWindow {
    int start_min = 0;
    int end_min = 0;
    bool operator==(const AvailabilityWindow& o) const { return start_min == o.start_min && end_min == o.end_min; }
};

// Weekday numbering used by config.working_weekdays: 0 = Monday ... 6 = Sunday.
inline int parseWeekdayName(const std::string& raw) {
    std::string n;
    for (const char c : raw) n += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static constexpr const char* full[] = {"monday", "tuesday", "wednesday", "thursday",
                                           "friday", "saturday", "sunday"};
    for (int i = 0; i < 7; ++i)
        if (n == full[i] || n == std::string(full[i]).substr(0, 3)) return i;
    return -1;
}
inline const char* weekdayName(int d) {
    static constexpr const char* names[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    return (d >= 0 && d < 7) ? names[d] : "?";
}

// Mirrors the "config" block of POST /api/v1/schedule/optimize.
// Times are minutes since midnight, exactly as in the frontend contract.
//
// Phase 4 additions (all optional; a Phase 1 config keeps its single-day meaning):
//   end_date             last day of the horizon (inclusive); default = target_date.
//   availability_windows several daily windows, e.g. 09:00-12:00 and 13:00-18:00. When present
//                        they REPLACE work_window_start_min/work_window_end_min. They must not
//                        overlap (touching windows are merged).
//   working_weekdays     weekdays on which windows apply ("mon".."sun"); default = every day.
struct SchedulingConfig {
    static constexpr int kMaxHorizonDays = 366;
    static constexpr size_t kMaxWindows = 12;

    std::string target_date;                       // first day of the horizon
    std::string end_date;                          // "" = same as target_date
    std::vector<AvailabilityWindow> availability_windows;   // empty = one window from work_window_*
    std::vector<int> working_weekdays;             // empty = all seven days
    int work_window_start_min = 510;   // 08:30
    int work_window_end_min = 1110;    // 18:30
    int min_break_min = 30;
    bool allow_splitting = true;
    int max_workload_min = 450;
    std::string objective = "balanced";

    void validate() const {
        if (!isValidDate(target_date))
            throw ValidationError("config.target_date must be a valid YYYY-MM-DD date");
        if (work_window_start_min < 0 || work_window_end_min > 1440 ||
            work_window_start_min >= work_window_end_min)
            throw ValidationError("config work window must satisfy 0 <= start < end <= 1440");
        const int window = work_window_end_min - work_window_start_min;
        if (min_break_min < 0 || min_break_min > 240 || min_break_min >= window)
            throw ValidationError("config.min_break_min must be 0-240 and shorter than the work window");
        if (max_workload_min < 1 || max_workload_min > 1440)
            throw ValidationError("config.max_workload_min must be between 1 and 1440");
        if (!isValidObjective(objective))
            throw ValidationError("config.objective must be one of: balanced, priority, deadline, compact, edf");

        // ---- Phase 4 fields
        if (!end_date.empty()) {
            if (!isValidDate(end_date)) throw ValidationError("config.end_date must be a valid YYYY-MM-DD date");
            if (end_date < target_date)   // YYYY-MM-DD compares correctly as text
                throw ValidationError("config.end_date must not be before config.target_date");
            if (horizonDays() > kMaxHorizonDays)
                throw ValidationError("config horizon must be at most " + std::to_string(kMaxHorizonDays) + " days");
        }
        if (availability_windows.size() > kMaxWindows)
            throw ValidationError("config.availability_windows may contain at most " +
                                  std::to_string(kMaxWindows) + " windows");
        for (const AvailabilityWindow& w : availability_windows)
            if (w.start_min < 0 || w.end_min > 1440 || w.start_min >= w.end_min)
                throw ValidationError("each availability window must satisfy 0 <= start_min < end_min <= 1440");
        {
            std::vector<AvailabilityWindow> sorted = availability_windows;
            std::sort(sorted.begin(), sorted.end(), [](const AvailabilityWindow& a, const AvailabilityWindow& b) {
                return a.start_min < b.start_min;
            });
            for (size_t i = 1; i < sorted.size(); ++i)
                if (sorted[i].start_min < sorted[i - 1].end_min)
                    throw ValidationError("config.availability_windows must not overlap");
        }
        std::set<int> seenDays;
        for (const int d : working_weekdays) {
            if (d < 0 || d > 6) throw ValidationError("config.working_weekdays contains an invalid weekday");
            if (!seenDays.insert(d).second)
                throw ValidationError("config.working_weekdays contains a duplicate weekday");
        }
    }

    // Number of calendar days in the horizon (1 for a single day). Assumes valid dates.
    int horizonDays() const {
        if (end_date.empty()) return 1;
        return static_cast<int>(dayNumber(end_date) - dayNumber(target_date)) + 1;
    }
    const std::string& lastDate() const { return end_date.empty() ? target_date : end_date; }

    // Days since 1970-01-01 for a valid YYYY-MM-DD (proleptic Gregorian, Howard Hinnant's algorithm).
    static long long dayNumber(const std::string& date) {
        int y = std::stoi(date.substr(0, 4));
        const int m = std::stoi(date.substr(5, 2));
        const int d = std::stoi(date.substr(8, 2));
        y -= m <= 2;
        const long long era = (y >= 0 ? y : y - 399) / 400;
        const long long yoe = y - era * 400;
        const long long doy = (153LL * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }

    // Phase 1 keys always; Phase 4 keys only when they were set (so old payloads round-trip).
    nlohmann::json toJson() const {
        nlohmann::json j = {{"target_date", target_date}, {"work_window_start_min", work_window_start_min},
                {"work_window_end_min", work_window_end_min}, {"min_break_min", min_break_min},
                {"allow_splitting", allow_splitting}, {"max_workload_min", max_workload_min},
                {"objective", objective}};
        if (!end_date.empty()) j["end_date"] = end_date;
        if (!availability_windows.empty()) {
            nlohmann::json w = nlohmann::json::array();
            for (const auto& x : availability_windows) w.push_back({{"start_min", x.start_min}, {"end_min", x.end_min}});
            j["availability_windows"] = w;
        }
        if (!working_weekdays.empty()) {
            nlohmann::json d = nlohmann::json::array();
            for (const int x : working_weekdays) d.push_back(weekdayName(x));
            j["working_weekdays"] = d;
        }
        return j;
    }

    static SchedulingConfig fromJson(const nlohmann::json& j) {
        requireObject(j, "config");
        SchedulingConfig c;
        c.target_date = getRequired<std::string>(j, "target_date");
        c.work_window_start_min = getOr<int>(j, "work_window_start_min", c.work_window_start_min);
        c.work_window_end_min = getOr<int>(j, "work_window_end_min", c.work_window_end_min);
        c.min_break_min = getOr<int>(j, "min_break_min", c.min_break_min);
        c.allow_splitting = getOr<bool>(j, "allow_splitting", c.allow_splitting);
        c.max_workload_min = getOr<int>(j, "max_workload_min", c.max_workload_min);
        c.objective = getOr<std::string>(j, "objective", c.objective);
        c.end_date = getOr<std::string>(j, "end_date", "");
        if (c.end_date.empty() && j.contains("end_date") && !j["end_date"].is_null())
            throw ValidationError("config.end_date must be a valid YYYY-MM-DD date");

        const auto wins = j.find("availability_windows");
        if (wins != j.end() && !wins->is_null()) {
            if (!wins->is_array()) throw ValidationError("field 'availability_windows' must be an array");
            for (const auto& w : *wins) {
                requireObject(w, "availability window");
                AvailabilityWindow aw;
                aw.start_min = getRequired<int>(w, "start_min");
                aw.end_min = getRequired<int>(w, "end_min");
                c.availability_windows.push_back(aw);
            }
        }
        const auto days = j.find("working_weekdays");
        if (days != j.end() && !days->is_null()) {
            if (!days->is_array()) throw ValidationError("field 'working_weekdays' must be an array");
            for (const auto& d : *days) {
                int n = -1;
                if (d.is_string()) n = parseWeekdayName(d.get<std::string>());
                else if (d.is_number_integer()) n = d.get<int>();
                if (n < 0 || n > 6)
                    throw ValidationError("config.working_weekdays entries must be weekday names (mon..sun)");
                c.working_weekdays.push_back(n);
            }
        }
        c.validate();
        return c;
    }
};

// Full body of POST /api/v1/schedule/optimize.
struct SchedulingRequest {
    std::string version = "1.0";
    SchedulingConfig config;
    std::vector<Event> fixed_events;
    std::vector<Task> flexible_tasks;

    static SchedulingRequest fromJson(const nlohmann::json& j) {
        requireObject(j, "request body");
        SchedulingRequest r;
        r.version = getOr<std::string>(j, "version", "1.0");
        const auto cfg = j.find("config");
        if (cfg == j.end()) throw ValidationError("missing required field 'config'");
        r.config = SchedulingConfig::fromJson(*cfg);

        for (const char* key : {"fixed_events", "flexible_tasks"}) {
            const auto it = j.find(key);
            if (it != j.end() && !it->is_null() && !it->is_array())
                throw ValidationError(std::string("field '") + key + "' must be an array");
        }
        if (j.contains("fixed_events") && j["fixed_events"].is_array())
            for (const auto& e : j["fixed_events"])
                r.fixed_events.push_back(Event::fromJson(e, r.config.target_date));
        if (j.contains("flexible_tasks") && j["flexible_tasks"].is_array())
            for (const auto& t : j["flexible_tasks"]) r.flexible_tasks.push_back(Task::fromJson(t));

        // Duplicate ids would corrupt dependency analysis later, so reject them here.
        std::set<std::string> taskIds;
        for (const Task& t : r.flexible_tasks)
            if (!t.id.empty() && !taskIds.insert(t.id.key()).second)
                throw ValidationError("duplicate task id '" + t.id.value + "' in flexible_tasks");
        std::set<std::string> eventIds;
        for (const Event& e : r.fixed_events)
            if (!e.id.empty() && !eventIds.insert(e.id.key()).second)
                throw ValidationError("duplicate event id '" + e.id.value + "' in fixed_events");
        return r;
    }
};

// ---- Result types (filled by the scheduler from Phase 5 onward) ----
//
// Conventions shared by every result object:
//  * Clock times are "HH:mm" strings in JSON (minutes internally; 1440 -> "24:00").
//  * Durations and buffers are integer minutes.
//  * Priority is an integer 1..5 everywhere (tasks, events, timeline rows).
//  * Every row has a string identifier: timeline "tl-N", slots "slot-N",
//    conflicts "cf-N"; unscheduled/at-risk rows are keyed by task_id.
//  * reference_id / task_id / event ids are `Id` (string OR integer, echoed
//    with the type the frontend used).
//
// ASSUMPTION - the enum values below are NOT confirmed against the frontend
// TypeScript. They are defined once, here, so a mismatch is a one-place fix.

enum class TimelineType { Event, Task };
enum class TimelineStatus { Fixed, Scheduled };
enum class ResultStatus { Success, Partial, Infeasible };
enum class RiskLevel { OnTrack, AtRisk, ImpossibleWithoutRescheduling };

inline const char* toString(TimelineType v) {
    switch (v) {
        case TimelineType::Event: return "event";
        case TimelineType::Task: return "task";
    }
    throw std::logic_error("unknown TimelineType");
}
inline const char* toString(TimelineStatus v) {
    switch (v) {
        case TimelineStatus::Fixed: return "fixed";
        case TimelineStatus::Scheduled: return "scheduled";
    }
    throw std::logic_error("unknown TimelineStatus");
}
inline const char* toString(ResultStatus v) {
    switch (v) {
        case ResultStatus::Success: return "success";
        case ResultStatus::Partial: return "partial";
        case ResultStatus::Infeasible: return "infeasible";
    }
    throw std::logic_error("unknown ResultStatus");
}
inline const char* toString(RiskLevel v) {
    switch (v) {
        case RiskLevel::OnTrack: return "on_track";
        case RiskLevel::AtRisk: return "at_risk";
        case RiskLevel::ImpossibleWithoutRescheduling: return "impossible_without_rescheduling";
    }
    throw std::logic_error("unknown RiskLevel");
}

struct TimelineEntry {
    // Phase 4 extras (NOT part of toJson(); emitted by toApiJson() so Phase 1 payloads are unchanged):
    std::string date;                        // YYYY-MM-DD of this row (multi-day horizons)
    int session_index = 0;                   // 1-based session number of a task (0 for events)
    int session_count = 0;                   // total sessions of that task (0 for events)

    std::string id;                          // "tl-N"
    TimelineType type = TimelineType::Task;
    Id reference_id;                         // id of the underlying event or task
    std::string title;
    int start_min = 0;
    int end_min = 0;
    int priority = 3;                        // 1..5
    TimelineStatus status = TimelineStatus::Scheduled;
    std::string explanation;                 // why it was placed here

    nlohmann::json toJson() const {
        return {{"id", id}, {"type", toString(type)}, {"reference_id", reference_id.toJson()},
                {"title", title}, {"start", formatHHmm(start_min)}, {"end", formatHHmm(end_min)},
                {"duration_min", end_min - start_min}, {"priority", priority},
                {"status", toString(status)}, {"explanation", explanation}};
    }
    nlohmann::json toApiJson() const {
        nlohmann::json j = toJson();
        j["date"] = date;
        j["session_index"] = session_index > 0 ? nlohmann::json(session_index) : nlohmann::json(nullptr);
        j["session_count"] = session_count > 0 ? nlohmann::json(session_count) : nlohmann::json(nullptr);
        return j;
    }
};

struct Slot {
    std::string date;                        // Phase 4 extra (toApiJson only)
    std::string id;                          // "slot-N"
    int start_min = 0;
    int end_min = 0;
    std::string label;
    nlohmann::json toJson() const {
        return {{"id", id}, {"start", formatHHmm(start_min)}, {"end", formatHHmm(end_min)},
                {"duration_min", end_min - start_min}, {"label", label}};
    }
    nlohmann::json toApiJson() const {
        nlohmann::json j = toJson();
        j["date"] = date;
        return j;
    }
};

struct UnscheduledTask {
    std::string reason_code;                 // Phase 4 extra (toApiJson only): machine-readable reason
    Id task_id;
    std::string title;
    int remaining_min = 0;                   // extra: work still unplaced
    std::string reason;
    std::string suggested_action;
    nlohmann::json toJson() const {
        return {{"task_id", task_id.toJson()}, {"title", title}, {"remaining_min", remaining_min},
                {"reason", reason}, {"suggested_action", suggested_action}};
    }
    nlohmann::json toApiJson() const {
        nlohmann::json j = toJson();
        j["reason_code"] = reason_code;
        return j;
    }
};

struct TaskAtRisk {
    // Phase 4 extras (toApiJson only).
    std::string category;                    // deadline_risk | hard_infeasible | dependency_blocked | insufficient_capacity
    std::optional<std::string> deadline;     // "YYYY-MM-DDTHH:mm" (24:00 = end of day); null = no deadline
    std::optional<std::string> estimated_completion;   // same format; null when nothing was fully placed

    Id task_id;
    std::string title;
    RiskLevel risk_level = RiskLevel::OnTrack;
    int buffer_min = 0;                      // available minus required; negative = shortfall
    std::string reason;
    int required_min = 0;                    // extras
    int available_min = 0;
    nlohmann::json toJson() const {
        return {{"task_id", task_id.toJson()}, {"title", title},
                {"risk_level", toString(risk_level)}, {"buffer_min", buffer_min},
                {"reason", reason}, {"required_min", required_min},
                {"available_min", available_min}};
    }
    nlohmann::json toApiJson() const {
        nlohmann::json j = toJson();
        j["category"] = category;
        j["deadline"] = optionalJson(deadline);
        j["estimated_completion"] = optionalJson(estimated_completion);
        return j;
    }
};

struct Conflict {
    std::string id;                          // "cf-N"
    Id event_a_id;
    Id event_b_id;
    std::string date;
    int overlap_start_min = 0;
    int overlap_end_min = 0;
    std::string message;
    nlohmann::json toJson() const {
        return {{"id", id}, {"event_a_id", event_a_id.toJson()}, {"event_b_id", event_b_id.toJson()},
                {"date", date}, {"overlap_start", formatHHmm(overlap_start_min)},
                {"overlap_end", formatHHmm(overlap_end_min)}, {"message", message}};
    }
};

// A hard-constraint violation found by the independent SchedulingValidator.
struct ScheduleViolation {
    std::string code;                        // e.g. "task_overlap", "dependency_order"
    std::string message;
    Id task_id;                              // empty when not about one task
    std::string date;                        // empty when not date-specific
    nlohmann::json toJson() const {
        return {{"code", code}, {"message", message}, {"task_id", task_id.toJson()}, {"date", date}};
    }
};

// How the numbers are defined is documented in PHASE4_DESIGN.md section 13.
struct SchedulingStatistics {
    int tasks_considered = 0;                // active tasks (not completed / cancelled)
    int tasks_scheduled = 0;                 // fully placed
    int tasks_partially_scheduled = 0;
    int tasks_unscheduled = 0;               // nothing placed
    int tasks_skipped_terminal = 0;          // completed or cancelled tasks, never placed
    int total_requested_min = 0;             // sum of duration_min of considered tasks
    int total_scheduled_min = 0;             // task minutes placed (sessions summed once each)
    int total_remaining_min = 0;             // requested - scheduled
    int fixed_events = 0;                    // events inside the horizon
    int total_window_min = 0;                // availability-window minutes over the horizon
    int fixed_event_window_min = 0;          // fixed-event minutes inside the windows
    int total_available_min = 0;             // window minutes minus fixed-event minutes
    int total_occupied_min = 0;              // scheduled task minutes + fixed-event window minutes
    double utilization_pct = 0.0;            // total_scheduled_min / total_available_min * 100
    int tasks_meeting_deadline = 0;          // fully placed tasks with a deadline, finishing on time
    int session_count = 0;

    nlohmann::json toJson() const {
        return {{"tasks_considered", tasks_considered}, {"tasks_scheduled", tasks_scheduled},
                {"tasks_partially_scheduled", tasks_partially_scheduled},
                {"tasks_unscheduled", tasks_unscheduled}, {"tasks_skipped_terminal", tasks_skipped_terminal},
                {"total_requested_min", total_requested_min}, {"total_scheduled_min", total_scheduled_min},
                {"total_remaining_min", total_remaining_min}, {"fixed_events", fixed_events},
                {"total_window_min", total_window_min}, {"fixed_event_window_min", fixed_event_window_min},
                {"total_available_min", total_available_min}, {"total_occupied_min", total_occupied_min},
                {"utilization_pct", utilization_pct}, {"tasks_meeting_deadline", tasks_meeting_deadline},
                {"session_count", session_count}};
    }
};

struct SchedulingResult {
    // Phase 4 extras (toApiJson only).
    std::string start_date;
    std::string end_date;
    std::string objective;
    SchedulingStatistics statistics;
    std::vector<ScheduleViolation> validation_errors;
    std::vector<std::string> warnings;

    std::string target_date;
    ResultStatus status = ResultStatus::Success;
    double solver_duration_ms = 0.0;
    int total_allocated_min = 0;
    int total_available_min = 0;
    std::vector<TimelineEntry> timeline;
    std::vector<Slot> available_slots;
    std::vector<UnscheduledTask> unscheduled_tasks;
    std::vector<TaskAtRisk> tasks_at_risk;
    std::vector<Conflict> conflicts;

    template <class T>
    static nlohmann::json arrayOf(const std::vector<T>& v) {
        nlohmann::json a = nlohmann::json::array();
        for (const auto& x : v) a.push_back(x.toJson());
        return a;
    }

    nlohmann::json toJson() const {
        return {{"target_date", target_date}, {"status", toString(status)},
                {"solver_duration_ms", solver_duration_ms},
                {"total_allocated_min", total_allocated_min},
                {"total_available_min", total_available_min},
                {"timeline", arrayOf(timeline)}, {"available_slots", arrayOf(available_slots)},
                {"unscheduled_tasks", arrayOf(unscheduled_tasks)},
                {"tasks_at_risk", arrayOf(tasks_at_risk)}, {"conflicts", arrayOf(conflicts)}};
    }

    template <class T>
    static nlohmann::json apiArrayOf(const std::vector<T>& v) {
        nlohmann::json a = nlohmann::json::array();
        for (const auto& x : v) a.push_back(x.toApiJson());
        return a;
    }

    // The Phase 1 shape (every key of toJson()) plus the Phase 4 keys. Nested rows use their own
    // toApiJson(). This is what POST /api/v1/schedule/optimize returns.
    nlohmann::json toApiJson() const {
        nlohmann::json j = toJson();
        j["start_date"] = start_date;
        j["end_date"] = end_date;
        j["objective"] = objective;
        j["timeline"] = apiArrayOf(timeline);
        j["available_slots"] = apiArrayOf(available_slots);
        j["unscheduled_tasks"] = apiArrayOf(unscheduled_tasks);
        j["tasks_at_risk"] = apiArrayOf(tasks_at_risk);
        j["statistics"] = statistics.toJson();
        j["validation_errors"] = arrayOf(validation_errors);
        nlohmann::json w = nlohmann::json::array();
        for (const auto& x : warnings) w.push_back(x);
        j["warnings"] = w;
        return j;
    }
};

}  // namespace chronoplan::models
