#pragma once
// TimeInterval: a half-open time range [start, end) measured in integer minutes,
// the same unit the models use (minutes since midnight, Event::start_min/end_min).
//
// Half-open convention:
//   [09:00,10:00) and [10:00,11:00) do NOT overlap (they only touch).
//   [09:00,11:00) and [10:00,12:00) overlap.
// Two ranges overlap iff  a.start < b.end  &&  b.start < a.end.
#include <string>
#include "algorithms/IdOrdering.hpp"
#include "models/Validation.hpp"

namespace chronoplan::algorithms {

enum class IntervalType { Event = 0, ScheduledTask = 1 };

inline const char* toString(IntervalType t) { return t == IntervalType::Event ? "event" : "scheduled_task"; }

struct TimeInterval {
    int start = 0;
    int end = 0;
    models::Id id;                          // id of the owning event / task
    IntervalType type = IntervalType::Event;

    // A default-constructed interval is an empty placeholder (invalid until assigned).
    TimeInterval() = default;
    // Throws models::ValidationError unless start < end and id is non-empty.
    TimeInterval(int s, int e, models::Id i, IntervalType t = IntervalType::Event)
        : start(s), end(e), id(std::move(i)), type(t) { validate(); }

    static bool isValidRange(int s, int e) { return s < e; }

    void validate() const {
        if (!isValidRange(start, end))
            throw models::ValidationError("interval start (" + std::to_string(start) +
                                          ") must be before end (" + std::to_string(end) + ")");
        if (id.empty()) throw models::ValidationError("interval id must not be empty");
    }

    int length() const { return end - start; }

    bool overlaps(int s, int e) const { return start < e && s < end; }
    bool overlaps(const TimeInterval& o) const { return overlaps(o.start, o.end); }

    bool operator==(const TimeInterval& o) const {
        return start == o.start && end == o.end && type == o.type && id == o.id;
    }
};

// Total order used by the IntervalTree: (start, end, type, id). Returns <0, 0, >0.
inline int compareIntervals(const TimeInterval& a, const TimeInterval& b) {
    if (a.start != b.start) return a.start < b.start ? -1 : 1;
    if (a.end != b.end) return a.end < b.end ? -1 : 1;
    if (a.type != b.type) return a.type < b.type ? -1 : 1;
    return compareIds(a.id, b.id);
}

// A free (unoccupied) range, also half-open.
struct TimeSlot {
    int start = 0;
    int end = 0;
    int length() const { return end - start; }
    bool operator==(const TimeSlot& o) const { return start == o.start && end == o.end; }
};

}  // namespace chronoplan::algorithms
