#pragma once
// Helpers shared by the Phase 3 algorithm tests (no database, no JSON needed).
#include <string>
#include "algorithms/TimeInterval.hpp"
#include "models/Validation.hpp"

namespace algotest {

inline chronoplan::models::Id num(long long v) {
    chronoplan::models::Id id; id.value = std::to_string(v); id.numeric = true; return id;
}
inline chronoplan::models::Id str(const std::string& v) {
    chronoplan::models::Id id; id.value = v; id.numeric = false; return id;
}
inline chronoplan::algorithms::TimeInterval iv(int s, int e, long long id = 1,
        chronoplan::algorithms::IntervalType t = chronoplan::algorithms::IntervalType::Event) {
    return chronoplan::algorithms::TimeInterval(s, e, num(id), t);
}
constexpr int H(int h, int m = 0) { return h * 60 + m; }   // HH:MM -> minutes

}  // namespace algotest
