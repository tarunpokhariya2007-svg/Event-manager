#include "scheduler/SchedulingConstraints.hpp"
#include <algorithm>

namespace chronoplan::scheduler {

SchedulingConstraints SchedulingConstraints::fromConfig(const models::SchedulingConfig& config) {
    config.validate();
    SchedulingConstraints c;
    c.objective = parseObjective(config.objective);
    c.start_date = config.target_date;
    c.end_date = config.lastDate();
    c.start_day = dayNumberOf(c.start_date);
    c.day_count = config.horizonDays();
    c.min_break_min = config.min_break_min;
    c.max_workload_min = config.max_workload_min;
    c.allow_splitting = config.allow_splitting;

    std::vector<models::AvailabilityWindow> w = config.availability_windows;
    if (w.empty()) w.push_back({config.work_window_start_min, config.work_window_end_min});
    std::sort(w.begin(), w.end(), [](const auto& a, const auto& b) { return a.start_min < b.start_min; });
    for (const auto& x : w) {
        if (!c.windows.empty() && c.windows.back().end_min == x.start_min)
            c.windows.back().end_min = x.end_min;          // touching windows form one window
        else
            c.windows.push_back(x);
    }
    if (!config.working_weekdays.empty()) {
        c.weekday_active.fill(false);
        for (const int d : config.working_weekdays) c.weekday_active[static_cast<size_t>(d)] = true;
    }
    return c;
}

}  // namespace chronoplan::scheduler
