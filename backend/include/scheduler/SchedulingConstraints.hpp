#pragma once
// Normalised, validated scheduling horizon + rules derived from models::SchedulingConfig.
#include <array>
#include <string>
#include <vector>
#include "models/Schedule.hpp"
#include "scheduler/CalendarDate.hpp"
#include "scheduler/SchedulingObjective.hpp"

namespace chronoplan::scheduler {

struct SchedulingConstraints {
    Objective objective = Objective::Balanced;
    std::string start_date;                         // first day of the horizon
    std::string end_date;                           // last day (inclusive)
    long long start_day = 0;                        // day number of start_date
    int day_count = 1;
    std::vector<models::AvailabilityWindow> windows;   // sorted, non-overlapping, touching ones merged
    std::array<bool, 7> weekday_active{{true, true, true, true, true, true, true}};   // 0 = Monday
    int min_break_min = 0;                          // minimum gap between two task sessions
    int max_workload_min = 1440;                    // task minutes per day
    bool allow_splitting = true;                    // global switch, ANDed with Task::allow_split

    long long horizonEndAbs() const { return static_cast<long long>(day_count) * kMinutesPerDay; }
    long long dateOfDayIndex(int dayIndex) const { return start_day + dayIndex; }
    std::string dateString(int dayIndex) const { return formatDayNumber(start_day + dayIndex); }
    bool isActiveDay(int dayIndex) const {
        return weekday_active[static_cast<size_t>(weekdayOfDayNumber(start_day + dayIndex))];
    }
    int windowMinutesPerDay() const {
        int s = 0;
        for (const auto& w : windows) s += w.end_min - w.start_min;
        return s;
    }

    // Validates the config first (throws models::ValidationError).
    static SchedulingConstraints fromConfig(const models::SchedulingConfig& config);
};

}  // namespace chronoplan::scheduler
