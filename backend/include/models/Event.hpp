#pragma once
#include <string>
#include "models/Validation.hpp"

namespace chronoplan::models {

// Internal event type. The frontend sends a boolean `is_flexible`; it is mapped
// to this enum on input and mapped back on output.
enum class EventKind { Fixed, Flexible };

inline const char* toString(EventKind k) { return k == EventKind::Fixed ? "fixed" : "flexible"; }

// A time-bound commitment on a single date. Events crossing midnight are NOT
// supported yet: end_min must be > start_min within the same day.
struct Event {
    Id id;
    std::string title;
    std::string date;          // YYYY-MM-DD
    int start_min = 0;         // 0..1439
    int end_min = 0;           // 1..1440, > start_min
    int priority = 3;          // 1..5
    EventKind kind = EventKind::Fixed;   // from is_flexible
    std::string category;      // optional label, e.g. "work"; empty = none
    std::string location;      // optional free text; empty = none

    int duration() const { return end_min - start_min; }
    bool isFlexible() const { return kind == EventKind::Flexible; }

    // Category is a short label, not an enum, because the frontend owns the list:
    // 1-50 chars of letters, digits, space, '-', '_' or '&'.
    static bool isValidCategory(const std::string& c) {
        if (c.empty() || c.size() > 50) return false;
        for (const char ch : c) {
            const unsigned char u = static_cast<unsigned char>(ch);
            if (!(std::isalnum(u) || ch == ' ' || ch == '-' || ch == '_' || ch == '&')) return false;
        }
        return true;
    }

    void validate() const {
        if (title.empty() || title.size() > 200)
            throw ValidationError("event title must be 1-200 characters");
        if (!isValidDate(date))
            throw ValidationError("event date must be a valid YYYY-MM-DD date");
        if (start_min < 0 || start_min > 1439)
            throw ValidationError("event start_min must be between 0 and 1439");
        if (end_min < 1 || end_min > 1440)
            throw ValidationError("event end_min must be between 1 and 1440");
        if (start_min >= end_min)
            throw ValidationError("event start_min must be before end_min");
        if (priority < 1 || priority > 5)
            throw ValidationError("event priority must be between 1 and 5");
        if (!category.empty() && !isValidCategory(category))
            throw ValidationError("event category must be 1-50 characters of letters, digits, "
                                  "space, '-', '_' or '&'");
        if (location.size() > 200)
            throw ValidationError("event location must be at most 200 characters");
    }

    // Exactly the fields the frontend expects.
    nlohmann::json toJson() const {
        return {{"id", id.toJson()}, {"title", title}, {"date", date},
                {"start_min", start_min}, {"end_min", end_min}, {"priority", priority},
                {"is_flexible", isFlexible()}, {"category", category}, {"location", location}};
    }

    // `defaultDate` is used when the payload has no "date" (e.g. fixed_events
    // inside an optimize request, whose date is config.target_date).
    // A legacy "kind": "fixed"|"flexible" is still accepted when is_flexible is
    // absent; if both are present they must agree.
    static Event fromJson(const nlohmann::json& j, const std::string& defaultDate = "") {
        requireObject(j, "event");
        Event e;
        e.id = getIdOr(j, "id");
        e.title = getRequired<std::string>(j, "title");
        e.date = getOr<std::string>(j, "date", defaultDate);
        e.start_min = getRequired<int>(j, "start_min");
        e.end_min = getRequired<int>(j, "end_min");
        e.priority = getOr<int>(j, "priority", 3);
        e.category = getOr<std::string>(j, "category", "");
        e.location = getOr<std::string>(j, "location", "");

        const bool hasFlag = j.contains("is_flexible") && !j["is_flexible"].is_null();
        bool flexible = getOr<bool>(j, "is_flexible", false);
        if (j.contains("kind") && !j["kind"].is_null()) {
            const std::string k = getOr<std::string>(j, "kind", "");
            if (k != "fixed" && k != "flexible")
                throw ValidationError("event kind must be 'fixed' or 'flexible'");
            const bool kindFlexible = (k == "flexible");
            if (hasFlag && kindFlexible != flexible)
                throw ValidationError("event fields 'is_flexible' and 'kind' disagree");
            flexible = kindFlexible;
        }
        e.kind = flexible ? EventKind::Flexible : EventKind::Fixed;

        e.validate();
        return e;
    }
};

}  // namespace chronoplan::models
