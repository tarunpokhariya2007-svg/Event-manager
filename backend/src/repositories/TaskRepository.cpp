#include "repositories/TaskRepository.hpp"
#include <unordered_map>
#include "database/SQLiteStatement.hpp"
#include "database/SQLiteTransaction.hpp"
#include "repositories/RepositoryErrors.hpp"
#include "repositories/RepositoryUtil.hpp"

namespace chronoplan::repositories {

using database::SQLiteStatement;
using database::SQLiteTransaction;
using models::Id;
using models::Task;

namespace {

constexpr const char* kSelect =
    "SELECT id_type, id_value, title, description, technical_context, priority, duration_min, "
    "min_chunk_min, allow_split, deadline_date, deadline_min, earliest_start_min, "
    "latest_completion_min, status FROM tasks";

// Binds the 12 non-id columns starting at parameter `first`.
void bindBody(SQLiteStatement& s, const Task& t, int first) {
    int i = first;
    s.bindText(i++, t.title);
    s.bindOptText(i++, t.description);
    s.bindOptText(i++, t.technical_context);
    s.bindInt(i++, t.priority);
    s.bindInt(i++, t.duration_min);
    s.bindInt(i++, t.min_chunk_min);
    s.bindInt(i++, t.allow_split ? 1 : 0);
    s.bindOptText(i++, t.deadline_date);
    s.bindInt(i++, t.deadline_min);
    s.bindOptInt(i++, t.earliest_start_min);
    s.bindOptInt(i++, t.latest_completion_min);
    s.bindOptText(i++, t.status);
}

Task taskFromRow(const SQLiteStatement& s) {
    Task t;
    t.id = readId(s, 0);
    t.title = s.columnText(2);
    t.description = s.columnOptText(3);
    t.technical_context = s.columnOptText(4);
    t.priority = s.columnInt(5);
    t.duration_min = s.columnInt(6);
    t.min_chunk_min = s.columnInt(7);
    t.allow_split = s.columnInt(8) != 0;
    t.deadline_date = s.columnOptText(9);
    t.deadline_min = s.columnInt(10);
    t.earliest_start_min = s.columnOptInt(11);
    t.latest_completion_min = s.columnOptInt(12);
    t.status = s.columnOptText(13);
    return t;
}

}  // namespace

Id TaskRepository::nextNumericId() {
    SQLiteStatement s = db_.prepare(
        "SELECT COALESCE(MAX(CAST(id_value AS INTEGER)), 0) + 1 FROM tasks "
        "WHERE user_id = ? AND id_type = 'number'");
    s.bindInt(1, userId_);
    s.step();
    Id id;
    id.numeric = true;
    id.value = std::to_string(s.columnInt64(0));
    return id;
}

bool TaskRepository::taskExists(const Id& id) {
    requireValidId(id, "task id");
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare("SELECT 1 FROM tasks WHERE user_id = ? AND id_type = ? AND id_value = ?");
    s.bindInt(1, userId_);
    bindId(s, 2, id);
    return s.step();
}

Task TaskRepository::createTask(Task task) {
    task.validate();
    SQLiteTransaction tx(db_);   // BEGIN IMMEDIATE: id assignment + insert cannot race
    if (task.id.empty()) {
        task.id = nextNumericId();
    } else if (taskExists(task.id)) {
        throw ConflictError("a task with id " + describeId(task.id) + " already exists");
    }
    task.validate();   // re-check now that the id (and the self-prerequisite rule) is known

    SQLiteStatement ins = db_.prepare(
        "INSERT INTO tasks (user_id, id_type, id_value, title, description, technical_context, priority, "
        "duration_min, min_chunk_min, allow_split, deadline_date, deadline_min, earliest_start_min, "
        "latest_completion_min, status) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    ins.bindInt(1, userId_);
    bindId(ins, 2, task.id);
    bindBody(ins, task, 4);
    try {
        ins.execute();
    } catch (const database::DatabaseError& e) {
        if (e.isConstraint()) throw ConflictError("task conflicts with stored data");
        throw;
    }
    deps_.replacePrerequisites(task.id, task.prerequisite_ids);
    tx.commit();
    return task;
}

std::optional<Task> TaskRepository::getTaskById(const Id& id) {
    requireValidId(id, "task id");
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(std::string(kSelect) + " WHERE user_id = ? AND id_type = ? AND id_value = ?");
    s.bindInt(1, userId_);
    bindId(s, 2, id);
    if (!s.step()) return std::nullopt;
    Task t = taskFromRow(s);
    t.prerequisite_ids = deps_.listPrerequisites(t.id);
    return t;
}

std::vector<Task> TaskRepository::getAllTasks() {
    auto lock = db_.lock();
    // Two queries total (no N+1): all tasks, then the whole edge list.
    std::unordered_map<std::string, std::vector<Id>> prereqs;
    for (const Dependency& d : deps_.getAllDependencies()) prereqs[d.task.key()].push_back(d.prerequisite);

    SQLiteStatement s = db_.prepare(std::string(kSelect) + " WHERE user_id = ? ORDER BY rowid");
    s.bindInt(1, userId_);
    std::vector<Task> out;
    while (s.step()) {
        Task t = taskFromRow(s);
        if (auto it = prereqs.find(t.id.key()); it != prereqs.end()) t.prerequisite_ids = it->second;
        out.push_back(std::move(t));
    }
    return out;
}

Task TaskRepository::updateTask(const Id& id, Task task) {
    requireValidId(id, "task id");
    task.id = id;
    task.validate();
    SQLiteTransaction tx(db_);
    SQLiteStatement upd = db_.prepare(
        "UPDATE tasks SET title = ?, description = ?, technical_context = ?, priority = ?, "
        "duration_min = ?, min_chunk_min = ?, allow_split = ?, deadline_date = ?, deadline_min = ?, "
        "earliest_start_min = ?, latest_completion_min = ?, status = ? "
        "WHERE user_id = ? AND id_type = ? AND id_value = ?");
    bindBody(upd, task, 1);
    upd.bindInt(13, userId_);
    bindId(upd, 14, id);
    upd.execute();
    if (db_.changes() == 0) throw NotFoundError("task " + describeId(id) + " not found");
    deps_.replacePrerequisites(id, task.prerequisite_ids);
    tx.commit();
    return task;
}

void TaskRepository::deleteTask(const Id& id) {
    requireValidId(id, "task id");
    SQLiteTransaction tx(db_);
    SQLiteStatement del = db_.prepare("DELETE FROM tasks WHERE user_id = ? AND id_type = ? AND id_value = ?");
    del.bindInt(1, userId_);
    bindId(del, 2, id);
    del.execute();   // task_dependencies rows (both directions) go via ON DELETE CASCADE
    if (db_.changes() == 0) throw NotFoundError("task " + describeId(id) + " not found");
    tx.commit();
}

}  // namespace chronoplan::repositories
