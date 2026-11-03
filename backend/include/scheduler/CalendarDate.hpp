#pragma once
// Calendar helpers for the scheduling engine. Dates are local calendar dates (no time zones).
// A "day number" is days since 1970-01-01; "absolute minutes" are minutes since 00:00 of the
// first day of the scheduling horizon (day index * 1440 + minute of day).
#include <cstdio>
#include <string>
#include "models/Schedule.hpp"

namespace chronoplan::scheduler {

constexpr int kMinutesPerDay = 1440;

inline long long dayNumberOf(const std::string& date) { return models::SchedulingConfig::dayNumber(date); }

// Inverse of dayNumberOf (Howard Hinnant's civil_from_days).
inline std::string formatDayNumber(long long z) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = yoe + era * 400;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    const int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    if (m <= 2) ++y;
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", static_cast<int>(y), m, d);
    return buf;
}

// 0 = Monday ... 6 = Sunday (1970-01-01 was a Thursday).
inline int weekdayOfDayNumber(long long days) {
    const long long w = (days + 3) % 7;
    return static_cast<int>(w < 0 ? w + 7 : w);
}

// Floor division / modulo that behave for negative numerators.
inline long long floorDiv(long long a, long long b) {
    long long q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

inline std::string dateTimeString(const std::string& date, int minute) {
    return date + "T" + models::formatHHmm(minute);
}

}  // namespace chronoplan::scheduler
