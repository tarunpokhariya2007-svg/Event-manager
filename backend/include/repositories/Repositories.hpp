#pragma once
#include "repositories/DependencyRepository.hpp"
#include "repositories/EventRepository.hpp"
#include "repositories/TaskRepository.hpp"

namespace chronoplan::repositories {

// All repositories over one Database, for ONE user. The API layer (and the scheduling
// service) depends on this, never on SQLite directly; build one per authenticated request.
struct Repositories {
    Repositories(database::Database& db, UserId userId)
        : tasks(db, userId), events(db, userId), dependencies(db, userId) {}
    TaskRepository tasks;
    EventRepository events;
    DependencyRepository dependencies;
};

}  // namespace chronoplan::repositories
