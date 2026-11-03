#pragma once
// SchedulingService: the only layer between the REST route and the scheduling engine that talks
// to repositories. It resolves where the tasks/events come from, rejects invalid input, runs the
// engine and refuses to return a schedule that failed independent validation.
//
// Data sources (request field "data_source"):
//   "request"  (default) tasks and events are taken from the body (flexible_tasks / fixed_events),
//              exactly like the Phase 1 contract. The database is not read at all.
//   "database" tasks (with their stored prerequisites) and all stored events inside the horizon
//              are loaded once through SchedulingDataProvider. flexible_tasks / fixed_events must
//              be absent or empty (mixing sources is ambiguous, so it is rejected).
// Both modes are READ-ONLY: a preview never writes to the database.
#include <nlohmann/json.hpp>
#include "models/Schedule.hpp"
#include "repositories/Repositories.hpp"

namespace chronoplan::services {

class SchedulingService {
public:
    explicit SchedulingService(repositories::Repositories& repos) : repos_(repos) {}

    // Throws models::ValidationError (400), repositories::CircularDependencyError (409),
    // database::DatabaseError (500/503) or std::runtime_error when the engine produced a
    // schedule that fails validation (500; indicates an engine bug).
    models::SchedulingResult optimize(const nlohmann::json& body);

private:
    repositories::Repositories& repos_;
};

}  // namespace chronoplan::services
