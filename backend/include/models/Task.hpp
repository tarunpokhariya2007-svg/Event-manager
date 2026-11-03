#pragma once
#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "models/Validation.hpp"

namespace chronoplan::models {

// A flexible unit of work the scheduler places into free time.
// All *_min values are minutes since midnight (0..1440) unless stated otherwise.
// Optional fields are std::nullopt when absent and serialise as JSON null.
struct Task {
    static constexpr int kMaxPrerequisites = 200;
    static constexpr size_t kMaxTextLength = 2000;

    Id id;                                  // empty = not yet persisted
    std::string title;
    std::optional<std::string> description;
    std::optional<std::string> technical_context;   // kept separate from description so
                                                    // whichever name the frontend uses survives
    int priority = 3;                       // 1 (lowest) .. 5 (highest)
    int duration_min = 0;                   // total work required, 1..1440
    int min_chunk_min = 30;                 // smallest allowed piece when splitting
    bool allow_split = true;
    std::optional<std::string> deadline_date;       // YYYY-MM-DD
    int deadline_min = 1440;                // minute of deadline_date by which work must end
    std::optional<int> earliest_start_min;          // 0..1439; work may not start before this
    std::optional<int> latest_completion_min;       // 1..1440; work must end by this
    std::vector<Id> prerequisite_ids;       // kept verbatim
    std::optional<std::string> status;      // opaque token; the backend does not yet know the
                                            // frontend's status list (see API_CONTRACT.md)

    // A chunk may never be longer than the task, and never shorter than 5 minutes
    // unless the whole task is shorter than 5 minutes (then the chunk IS the task).
    static int minChunkLowerBound(int duration) { return (std::min)(5, duration); }
    static int defaultMinChunk(int duration) { return (std::min)(30, duration); }

    // Format check only (1-32 chars: letters, digits, space, '_' or '-').
    // Which statuses exist is the frontend's decision and is NOT enforced here.
    static bool isValidStatus(const std::string& s) {
        if (s.empty() || s.size() > 32) return false;
        for (const char c : s) {
            const unsigned char u = static_cast<unsigned char>(c);
            if (!(std::isalnum(u) || c == ' ' || c == '_' || c == '-')) return false;
        }
        return true;
    }

    void validate() const {
        if (title.empty() || title.size() > 200)
            throw ValidationError("task title must be 1-200 characters");
        if (description && description->size() > kMaxTextLength)
            throw ValidationError("task description must be at most 2000 characters");
        if (technical_context && technical_context->size() > kMaxTextLength)
            throw ValidationError("task technical_context must be at most 2000 characters");
        if (priority < 1 || priority > 5)
            throw ValidationError("task priority must be between 1 and 5");
        if (duration_min < 1 || duration_min > 1440)
            throw ValidationError("task duration_min must be between 1 and 1440");
        if (min_chunk_min < minChunkLowerBound(duration_min) || min_chunk_min > duration_min)
            throw ValidationError("task min_chunk_min must be between " +
                                  std::to_string(minChunkLowerBound(duration_min)) + " and duration_min");
        if (deadline_min < 0 || deadline_min > 1440)
            throw ValidationError("task deadline_min must be between 0 and 1440");
        if (deadline_date && !isValidDate(*deadline_date))
            throw ValidationError("task deadline_date must be a valid YYYY-MM-DD date");

        if (earliest_start_min && (*earliest_start_min < 0 || *earliest_start_min > 1439))
            throw ValidationError("task earliest_start_min must be between 0 and 1439");
        if (latest_completion_min && (*latest_completion_min < 1 || *latest_completion_min > 1440))
            throw ValidationError("task latest_completion_min must be between 1 and 1440");
        // A window shorter than duration_min is NOT invalid input: the scheduler must
        // report such a task as unscheduled/at risk instead of rejecting the request.
        if (earliest_start_min && latest_completion_min && *latest_completion_min <= *earliest_start_min)
            throw ValidationError("task latest_completion_min must be after earliest_start_min");

        if (status && !isValidStatus(*status))
            throw ValidationError("task status must be 1-32 characters of letters, digits, "
                                  "space, '_' or '-'");

        if (static_cast<int>(prerequisite_ids.size()) > kMaxPrerequisites)
            throw ValidationError("a task may have at most 200 prerequisite_ids");
        std::set<std::string> seen;
        for (const Id& p : prerequisite_ids) {
            if (p.empty() || (!p.numeric && !isValidIdText(p.value)))
                throw ValidationError("prerequisite_ids contains an invalid id");
            if (!seen.insert(p.key()).second)
                throw ValidationError("prerequisite_ids contains duplicate id '" + p.value + "'");
            if (!id.empty() && p == id)
                throw ValidationError("task '" + id.value + "' cannot be its own prerequisite");
        }
    }

    nlohmann::json toJson() const {
        nlohmann::json prereqs = nlohmann::json::array();
        for (const Id& p : prerequisite_ids) prereqs.push_back(p.toJson());
        return {{"id", id.toJson()},
                {"title", title},
                {"description", optionalJson(description)},
                {"technical_context", optionalJson(technical_context)},
                {"priority", priority},
                {"duration_min", duration_min},
                {"min_chunk_min", min_chunk_min},
                {"allow_split", allow_split},
                {"deadline_date", optionalJson(deadline_date)},
                {"deadline_min", deadline_min},
                {"earliest_start_min", optionalJson(earliest_start_min)},
                {"latest_completion_min", optionalJson(latest_completion_min)},
                {"prerequisite_ids", prereqs},
                {"status", optionalJson(status)}};
    }

    // Parses and validates. Throws ValidationError on any problem.
    static Task fromJson(const nlohmann::json& j) {
        requireObject(j, "task");
        Task t;
        t.id = getIdOr(j, "id");
        t.title = getRequired<std::string>(j, "title");
        t.description = getOptional<std::string>(j, "description");
        t.technical_context = getOptional<std::string>(j, "technical_context");
        t.priority = getOr<int>(j, "priority", 3);
        t.duration_min = getRequired<int>(j, "duration_min");
        t.allow_split = getOr<bool>(j, "allow_split", true);
        t.min_chunk_min = getOr<int>(j, "min_chunk_min", defaultMinChunk(t.duration_min));
        t.deadline_date = getOptional<std::string>(j, "deadline_date");
        t.deadline_min = getOr<int>(j, "deadline_min", 1440);
        t.earliest_start_min = getOptional<int>(j, "earliest_start_min");
        t.latest_completion_min = getOptional<int>(j, "latest_completion_min");
        t.status = getOptional<std::string>(j, "status");

        const auto pr = j.find("prerequisite_ids");
        if (pr != j.end() && !pr->is_null()) {
            if (!pr->is_array()) throw ValidationError("field 'prerequisite_ids' must be an array");
            for (const auto& item : *pr) t.prerequisite_ids.push_back(parseId(item, "prerequisite_ids"));
        }

        t.validate();
        return t;
    }
};

}  // namespace chronoplan::models
