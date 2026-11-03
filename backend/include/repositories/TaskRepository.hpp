#pragma once
#include <optional>
#include <vector>
#include "database/Database.hpp"
#include "models/Task.hpp"
#include "repositories/DependencyRepository.hpp"
#include "repositories/UserId.hpp"

namespace chronoplan::repositories {

class TaskRepository {
public:
    // All operations are restricted to the rows owned by `userId`.
    TaskRepository(database::Database& db, UserId userId) : db_(db), userId_(userId), deps_(db, userId) {}

    // Validates, assigns a numeric id when t.id is empty, stores the task and its
    // prerequisite_ids atomically. Returns the stored task. ConflictError on duplicate id.
    models::Task createTask(models::Task task);

    std::optional<models::Task> getTaskById(const models::Id& id);
    std::vector<models::Task> getAllTasks();   // insertion order, prerequisites filled in

    // FULL REPLACEMENT of every field of task `id` (absent optionals become NULL,
    // prerequisite_ids replaces the stored set). Atomic. NotFoundError if missing.
    models::Task updateTask(const models::Id& id, models::Task task);

    // Also removes every dependency row that references the task (ON DELETE CASCADE).
    void deleteTask(const models::Id& id);     // NotFoundError if missing
    bool taskExists(const models::Id& id);

private:
    models::Id nextNumericId();

    database::Database& db_;
    UserId userId_;
    DependencyRepository deps_;
};

}  // namespace chronoplan::repositories
