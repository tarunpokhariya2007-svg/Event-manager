#pragma once
#include <optional>
#include <string>
#include <vector>
#include "database/Database.hpp"
#include "models/Event.hpp"
#include "repositories/UserId.hpp"

namespace chronoplan::repositories {

class EventRepository {
public:
    // All operations are restricted to the rows owned by `userId`.
    EventRepository(database::Database& db, UserId userId) : db_(db), userId_(userId) {}

    models::Event createEvent(models::Event event);      // assigns a numeric id when empty
    std::optional<models::Event> getEventById(const models::Id& id);
    std::vector<models::Event> getAllEvents();           // insertion order
    models::Event updateEvent(const models::Id& id, models::Event event);   // full replacement
    void deleteEvent(const models::Id& id);              // NotFoundError if missing
    // `date` must be YYYY-MM-DD (ValidationError otherwise). Ordered by start time.
    std::vector<models::Event> getEventsByDate(const std::string& date);
    bool eventExists(const models::Id& id);

private:
    models::Id nextNumericId();

    database::Database& db_;
    UserId userId_;
};

}  // namespace chronoplan::repositories
