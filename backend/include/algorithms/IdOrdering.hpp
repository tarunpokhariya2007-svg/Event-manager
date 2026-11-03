#pragma once
// Deterministic total order over models::Id, shared by the Phase 3 algorithms.
//   * numeric ids sort before string ids;
//   * numeric ids compare by numeric value (so 2 < 10), string ids lexicographically;
//   * ids that tie on all of the above fall back to their text so the order is strict.
#include <string>
#include <string_view>
#include "models/Validation.hpp"

namespace chronoplan::algorithms {

inline std::string_view stripLeadingZeros(std::string_view s) {
    size_t i = 0;
    while (i + 1 < s.size() && s[i] == '0') ++i;
    return s.substr(i);
}

inline int compareIds(const models::Id& a, const models::Id& b) {
    if (a.numeric != b.numeric) return a.numeric ? -1 : 1;
    if (a.numeric) {
        const std::string_view x = stripLeadingZeros(a.value);
        const std::string_view y = stripLeadingZeros(b.value);
        if (x.size() != y.size()) return x.size() < y.size() ? -1 : 1;
        const int c = x.compare(y);
        if (c != 0) return c < 0 ? -1 : 1;
    }
    const int c = a.value.compare(b.value);
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

struct IdLess {
    bool operator()(const models::Id& a, const models::Id& b) const { return compareIds(a, b) < 0; }
};

// Type-aware unique key (numeric 7 and string "7" differ).
struct IdKey {
    std::string operator()(const models::Id& id) const { return id.key(); }
};

}  // namespace chronoplan::algorithms
