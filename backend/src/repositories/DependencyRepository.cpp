#include "repositories/DependencyRepository.hpp"
#include <set>
#include "database/SQLiteStatement.hpp"
#include "database/SQLiteTransaction.hpp"
#include "repositories/RepositoryErrors.hpp"
#include "repositories/RepositoryUtil.hpp"

namespace chronoplan::repositories {

using database::SQLiteStatement;
using database::SQLiteTransaction;
using models::Id;
using models::ValidationError;

bool DependencyRepository::taskExists(const Id& id) {
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare("SELECT 1 FROM tasks WHERE user_id = ? AND id_type = ? AND id_value = ?");
    s.bindInt(1, userId_);
    bindId(s, 2, id);
    return s.step();
}

bool DependencyRepository::edgeExists(const Id& task, const Id& prerequisite) {
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(
        "SELECT 1 FROM task_dependencies WHERE user_id = ? AND task_id_type = ? AND task_id_value = ? "
        "AND prerequisite_id_type = ? AND prerequisite_id_value = ?");
    s.bindInt(1, userId_);
    bindId(s, 2, task);
    bindId(s, 4, prerequisite);
    return s.step();
}

void DependencyRepository::insertEdge(const Id& task, const Id& prerequisite) {
    SQLiteStatement s = db_.prepare(
        "INSERT INTO task_dependencies (user_id, task_id_type, task_id_value, prerequisite_id_type, "
        "prerequisite_id_value) VALUES (?, ?, ?, ?, ?)");
    s.bindInt(1, userId_);
    bindId(s, 2, task);
    bindId(s, 4, prerequisite);
    try {
        s.execute();
    } catch (const database::DatabaseError& e) {
        if (e.isConstraint()) throw ConflictError("dependency already exists or violates a constraint");
        throw;
    }
}

void DependencyRepository::addDependency(const Id& task, const Id& prerequisite) {
    requireValidId(task, "task id");
    requireValidId(prerequisite, "prerequisite id");
    if (task == prerequisite)
        throw ValidationError("task " + describeId(task) + " cannot be its own prerequisite");

    SQLiteTransaction tx(db_);
    if (!taskExists(task)) throw NotFoundError("task " + describeId(task) + " not found");
    if (!taskExists(prerequisite))
        throw NotFoundError("prerequisite task " + describeId(prerequisite) + " not found");
    if (edgeExists(task, prerequisite))
        throw ConflictError("task " + describeId(task) + " already depends on " + describeId(prerequisite));
    if (hasCircularDependency(task, prerequisite))
        throw CircularDependencyError("adding this dependency would create a circular dependency");
    insertEdge(task, prerequisite);
    tx.commit();
}

void DependencyRepository::removeDependency(const Id& task, const Id& prerequisite) {
    requireValidId(task, "task id");
    requireValidId(prerequisite, "prerequisite id");
    SQLiteTransaction tx(db_);
    SQLiteStatement s = db_.prepare(
        "DELETE FROM task_dependencies WHERE user_id = ? AND task_id_type = ? AND task_id_value = ? "
        "AND prerequisite_id_type = ? AND prerequisite_id_value = ?");
    s.bindInt(1, userId_);
    bindId(s, 2, task);
    bindId(s, 4, prerequisite);
    s.execute();
    if (db_.changes() == 0)
        throw NotFoundError("task " + describeId(task) + " does not depend on " + describeId(prerequisite));
    tx.commit();
}

std::vector<Id> DependencyRepository::listPrerequisites(const Id& task) {
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(
        "SELECT prerequisite_id_type, prerequisite_id_value FROM task_dependencies "
        "WHERE user_id = ? AND task_id_type = ? AND task_id_value = ? ORDER BY rowid");
    s.bindInt(1, userId_);
    bindId(s, 2, task);
    std::vector<Id> out;
    while (s.step()) out.push_back(readId(s, 0));
    return out;
}

std::vector<Id> DependencyRepository::getPrerequisites(const Id& task) {
    requireValidId(task, "task id");
    auto lock = db_.lock();
    if (!taskExists(task)) throw NotFoundError("task " + describeId(task) + " not found");
    return listPrerequisites(task);
}

std::vector<Id> DependencyRepository::getDependents(const Id& task) {
    requireValidId(task, "task id");
    auto lock = db_.lock();
    if (!taskExists(task)) throw NotFoundError("task " + describeId(task) + " not found");
    SQLiteStatement s = db_.prepare(
        "SELECT task_id_type, task_id_value FROM task_dependencies "
        "WHERE user_id = ? AND prerequisite_id_type = ? AND prerequisite_id_value = ? ORDER BY rowid");
    s.bindInt(1, userId_);
    bindId(s, 2, task);
    std::vector<Id> out;
    while (s.step()) out.push_back(readId(s, 0));
    return out;
}

bool DependencyRepository::hasCircularDependency(const Id& task, const Id& prerequisite) {
    if (task == prerequisite) return true;
    auto lock = db_.lock();
    // New edge: task -> requires -> prerequisite. A cycle appears iff `prerequisite`
    // already (transitively) requires `task`. Walk prerequisites of `prerequisite`.
    SQLiteStatement next = db_.prepare(
        "SELECT prerequisite_id_type, prerequisite_id_value FROM task_dependencies "
        "WHERE user_id = ? AND task_id_type = ? AND task_id_value = ?");
    std::set<std::string> visited{prerequisite.key()};
    std::vector<Id> stack{prerequisite};
    while (!stack.empty()) {
        const Id current = stack.back();
        stack.pop_back();
        next.bindInt(1, userId_);   // reset() clears bindings, so bind every round
        bindId(next, 2, current);
        std::vector<Id> neighbours;
        while (next.step()) neighbours.push_back(readId(next, 0));
        next.reset();
        for (const Id& n : neighbours) {
            if (n == task) return true;
            if (visited.insert(n.key()).second) stack.push_back(n);
        }
    }
    return false;
}

std::vector<Dependency> DependencyRepository::getAllDependencies() {
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(
        "SELECT task_id_type, task_id_value, prerequisite_id_type, prerequisite_id_value "
        "FROM task_dependencies WHERE user_id = ? ORDER BY rowid");
    s.bindInt(1, userId_);
    std::vector<Dependency> out;
    while (s.step()) out.push_back({readId(s, 0), readId(s, 2)});
    return out;
}

void DependencyRepository::replacePrerequisites(const Id& task, const std::vector<Id>& prerequisites) {
    SQLiteTransaction tx(db_);
    for (const Id& p : prerequisites) {
        requireValidId(p, "prerequisite id");
        if (!taskExists(p))
            throw ValidationError("prerequisite task " + describeId(p) + " does not exist");
    }
    {
        SQLiteStatement del = db_.prepare(
            "DELETE FROM task_dependencies WHERE user_id = ? AND task_id_type = ? AND task_id_value = ?");
        del.bindInt(1, userId_);
        bindId(del, 2, task);
        del.execute();
    }
    for (const Id& p : prerequisites) {
        if (p == task) throw ValidationError("task " + describeId(task) + " cannot be its own prerequisite");
        if (hasCircularDependency(task, p))
            throw CircularDependencyError("prerequisite " + describeId(p) + " would create a circular dependency");
        insertEdge(task, p);
    }
    tx.commit();
}

}  // namespace chronoplan::repositories
