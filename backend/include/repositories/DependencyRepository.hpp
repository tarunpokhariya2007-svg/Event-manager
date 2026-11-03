#pragma once
#include <vector>
#include "database/Database.hpp"
#include "models/Validation.hpp"
#include "repositories/UserId.hpp"

namespace chronoplan::repositories {

// "task" requires "prerequisite" to be finished first (prerequisite -> task).
struct Dependency {
    models::Id task;
    models::Id prerequisite;
};

class DependencyRepository {
public:
    // All operations are restricted to the tasks/edges owned by `userId`.
    DependencyRepository(database::Database& db, UserId userId) : db_(db), userId_(userId) {}

    // Atomic. Throws ValidationError (self-dependency / bad id), NotFoundError (unknown task),
    // ConflictError (duplicate), CircularDependencyError (would create a cycle).
    void addDependency(const models::Id& task, const models::Id& prerequisite);
    // Throws NotFoundError when the edge does not exist.
    void removeDependency(const models::Id& task, const models::Id& prerequisite);

    // Both throw NotFoundError when `task` does not exist. Order = insertion order.
    std::vector<models::Id> getPrerequisites(const models::Id& task);
    std::vector<models::Id> getDependents(const models::Id& task);

    // True if adding "task requires prerequisite" would close a cycle (also true for
    // task == prerequisite). Iterative DFS over stored edges; works for chains of any length.
    bool hasCircularDependency(const models::Id& task, const models::Id& prerequisite);

    // All edges in insertion order: one query for the whole graph (Phase 3 scheduler input).
    std::vector<Dependency> getAllDependencies();

    // Used by TaskRepository inside its own transaction: replaces the whole prerequisite
    // set of an existing task. Unknown prerequisites -> ValidationError (they came from a
    // task body, so 400 rather than 404).
    void replacePrerequisites(const models::Id& task, const std::vector<models::Id>& prerequisites);

    // Prerequisites without the task-exists check (caller already knows it exists).
    std::vector<models::Id> listPrerequisites(const models::Id& task);

private:
    bool taskExists(const models::Id& id);
    bool edgeExists(const models::Id& task, const models::Id& prerequisite);
    void insertEdge(const models::Id& task, const models::Id& prerequisite);

    database::Database& db_;
    UserId userId_;
};

}  // namespace chronoplan::repositories
