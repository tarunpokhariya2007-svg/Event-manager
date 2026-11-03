#pragma once
// Shared validation helpers for the JSON models.
// All model parsing reports bad input via ValidationError, which the API layer
// converts into HTTP 400 responses (never into 500s).
#include <cctype>
#include <cstdio>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <nlohmann/json.hpp>

namespace chronoplan::models {

class ValidationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------- dates/times

// Strict YYYY-MM-DD check including month lengths and leap years. O(1).
inline bool isValidDate(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    const int y = std::stoi(s.substr(0, 4));
    const int m = std::stoi(s.substr(5, 2));
    const int d = std::stoi(s.substr(8, 2));
    if (m < 1 || m > 12 || d < 1) return false;
    static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    const int maxDay = days[m - 1] + ((m == 2 && leap) ? 1 : 0);
    return d <= maxDay;
}

// Minutes since midnight -> "HH:mm". 1440 (end of day) renders as "24:00". O(1).
inline std::string formatHHmm(int minutes) {
    if (minutes < 0 || minutes > 1440)
        throw ValidationError("minute value " + std::to_string(minutes) + " is outside 0..1440");
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02d:%02d", minutes / 60, minutes % 60);
    return buf;
}

// ------------------------------------------------------------------------ ids

// An entity identifier. The frontend may use numbers or strings, so the original
// JSON type is remembered and written back unchanged (no silent type changes).
// Valid formats: a non-negative integer up to 2^53-1, or a string of 1-64
// characters from [A-Za-z0-9 - _ : .].
struct Id {
    std::string value;       // canonical text form; empty = "not assigned yet"
    bool numeric = false;    // arrived as a JSON integer

    bool empty() const { return value.empty(); }
    // Same text AND same JSON type: numeric 7 and string "7" are different ids.
    bool operator==(const Id& other) const { return numeric == other.numeric && value == other.value; }

    // Type-aware key for std::set / std::map (never use `value` alone for identity).
    std::string key() const { return (numeric ? "n:" : "s:") + value; }

    nlohmann::json toJson() const {
        if (value.empty()) return nlohmann::json(nullptr);
        if (numeric) return nlohmann::json(std::stoll(value));
        return nlohmann::json(value);
    }
};

inline bool isValidIdText(const std::string& s) {
    if (s.empty() || s.size() > 64) return false;
    for (const char c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!(std::isalnum(u) || c == '-' || c == '_' || c == ':' || c == '.')) return false;
    }
    return true;
}

inline Id parseId(const nlohmann::json& v, const char* field) {
    constexpr unsigned long long kMaxSafeInt = 9007199254740991ULL;  // 2^53 - 1 (JS safe)
    const std::string name = field;
    Id id;
    if (v.is_number_integer()) {
        if (v.is_number_unsigned()) {
            const auto n = v.get<unsigned long long>();
            if (n > kMaxSafeInt) throw ValidationError(name + " contains an id that is too large");
            id.value = std::to_string(n);
        } else {
            const auto n = v.get<long long>();
            if (n < 0 || static_cast<unsigned long long>(n) > kMaxSafeInt)
                throw ValidationError(name + " contains an invalid numeric id");
            id.value = std::to_string(n);
        }
        id.numeric = true;
    } else if (v.is_string()) {
        id.value = v.get<std::string>();
        if (!isValidIdText(id.value))
            throw ValidationError(name + " contains an id with an invalid format "
                                         "(1-64 chars from A-Z a-z 0-9 - _ : .)");
    } else {
        throw ValidationError(name + " ids must be strings or non-negative integers");
    }
    return id;
}

// Optional id: missing/null -> empty Id.
inline Id getIdOr(const nlohmann::json& j, const char* key) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) return Id{};
    return parseId(*it, key);
}

// --------------------------------------------------------------- field access

// Converts a JSON value to T. Integer targets are strict: 30.5, "30" and values
// outside T's range are rejected rather than silently truncated.
template <class T>
T convertValue(const nlohmann::json& v, const char* key) {
    const std::string name = key;
    try {
        if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            if (!v.is_number_integer()) throw ValidationError("field '" + name + "' must be an integer");
            const long long n = v.get<long long>();
            if (n < static_cast<long long>((std::numeric_limits<T>::min)()) ||
                n > static_cast<long long>((std::numeric_limits<T>::max)()))
                throw ValidationError("field '" + name + "' is out of range");
            return static_cast<T>(n);
        } else {
            return v.get<T>();
        }
    } catch (const nlohmann::json::exception&) {
        throw ValidationError("field '" + name + "' has the wrong type");
    }
}

// Optional field: returns `fallback` when missing/null, throws on wrong type.
template <class T>
T getOr(const nlohmann::json& j, const char* key, const T& fallback) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) return fallback;
    return convertValue<T>(*it, key);
}

// Required field: throws when missing/null or of the wrong type.
template <class T>
T getRequired(const nlohmann::json& j, const char* key) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null())
        throw ValidationError(std::string("missing required field '") + key + "'");
    return convertValue<T>(*it, key);
}

// Optional field: nullopt when missing/null, throws on wrong type.
template <class T>
std::optional<T> getOptional(const nlohmann::json& j, const char* key) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) return std::nullopt;
    return convertValue<T>(*it, key);
}

// Unset optionals are written as JSON null so every key is always present.
template <class T>
nlohmann::json optionalJson(const std::optional<T>& v) {
    return v ? nlohmann::json(*v) : nlohmann::json(nullptr);
}

inline void requireObject(const nlohmann::json& j, const char* what) {
    if (!j.is_object()) throw ValidationError(std::string(what) + " must be a JSON object");
}

}  // namespace chronoplan::models
