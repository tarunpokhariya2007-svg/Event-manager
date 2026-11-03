#include "repositories/EventRepository.hpp"
#include "database/SQLiteStatement.hpp"
#include "database/SQLiteTransaction.hpp"
#include "repositories/RepositoryErrors.hpp"
#include "repositories/RepositoryUtil.hpp"

namespace chronoplan::repositories {

using database::SQLiteStatement;
using database::SQLiteTransaction;
using models::Event;
using models::EventKind;
using models::Id;

namespace {

constexpr const char* kSelect =
    "SELECT id_type, id_value, title, date, start_min, end_min, priority, is_flexible, category, "
    "location FROM events";

// Binds the 8 non-id columns starting at parameter `first`.
void bindBody(SQLiteStatement& s, const Event& e, int first) {
    int i = first;
    s.bindText(i++, e.title);
    s.bindText(i++, e.date);
    s.bindInt(i++, e.start_min);
    s.bindInt(i++, e.end_min);
    s.bindInt(i++, e.priority);
    s.bindInt(i++, e.isFlexible() ? 1 : 0);
    s.bindText(i++, e.category);
    s.bindText(i++, e.location);
}

Event eventFromRow(const SQLiteStatement& s) {
    Event e;
    e.id = readId(s, 0);
    e.title = s.columnText(2);
    e.date = s.columnText(3);
    e.start_min = s.columnInt(4);
    e.end_min = s.columnInt(5);
    e.priority = s.columnInt(6);
    e.kind = s.columnInt(7) != 0 ? EventKind::Flexible : EventKind::Fixed;
    e.category = s.columnText(8);
    e.location = s.columnText(9);
    return e;
}

}  // namespace

Id EventRepository::nextNumericId() {
    SQLiteStatement s = db_.prepare(
        "SELECT COALESCE(MAX(CAST(id_value AS INTEGER)), 0) + 1 FROM events "
        "WHERE user_id = ? AND id_type = 'number'");
    s.bindInt(1, userId_);
    s.step();
    Id id;
    id.numeric = true;
    id.value = std::to_string(s.columnInt64(0));
    return id;
}

bool EventRepository::eventExists(const Id& id) {
    requireValidId(id, "event id");
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare("SELECT 1 FROM events WHERE user_id = ? AND id_type = ? AND id_value = ?");
    s.bindInt(1, userId_);
    bindId(s, 2, id);
    return s.step();
}

Event EventRepository::createEvent(Event event) {
    event.validate();
    SQLiteTransaction tx(db_);
    if (event.id.empty()) {
        event.id = nextNumericId();
    } else if (eventExists(event.id)) {
        throw ConflictError("an event with id " + describeId(event.id) + " already exists");
    }
    SQLiteStatement ins = db_.prepare(
        "INSERT INTO events (user_id, id_type, id_value, title, date, start_min, end_min, priority, "
        "is_flexible, category, location) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    ins.bindInt(1, userId_);
    bindId(ins, 2, event.id);
    bindBody(ins, event, 4);
    try {
        ins.execute();
    } catch (const database::DatabaseError& e) {
        if (e.isConstraint()) throw ConflictError("event conflicts with stored data");
        throw;
    }
    tx.commit();
    return event;
}

std::optional<Event> EventRepository::getEventById(const Id& id) {
    requireValidId(id, "event id");
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(std::string(kSelect) + " WHERE user_id = ? AND id_type = ? AND id_value = ?");
    s.bindInt(1, userId_);
    bindId(s, 2, id);
    if (!s.step()) return std::nullopt;
    return eventFromRow(s);
}

std::vector<Event> EventRepository::getAllEvents() {
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(std::string(kSelect) + " WHERE user_id = ? ORDER BY rowid");
    s.bindInt(1, userId_);
    std::vector<Event> out;
    while (s.step()) out.push_back(eventFromRow(s));
    return out;
}

std::vector<Event> EventRepository::getEventsByDate(const std::string& date) {
    if (!models::isValidDate(date)) throw models::ValidationError("date must be a valid YYYY-MM-DD date");
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(std::string(kSelect) + " WHERE user_id = ? AND date = ? ORDER BY start_min, end_min, rowid");
    s.bindInt(1, userId_);
    s.bindText(2, date);
    std::vector<Event> out;
    while (s.step()) out.push_back(eventFromRow(s));
    return out;
}

Event EventRepository::updateEvent(const Id& id, Event event) {
    requireValidId(id, "event id");
    event.id = id;
    event.validate();
    SQLiteTransaction tx(db_);
    SQLiteStatement upd = db_.prepare(
        "UPDATE events SET title = ?, date = ?, start_min = ?, end_min = ?, priority = ?, "
        "is_flexible = ?, category = ?, location = ? WHERE user_id = ? AND id_type = ? AND id_value = ?");
    bindBody(upd, event, 1);
    upd.bindInt(9, userId_);
    bindId(upd, 10, id);
    upd.execute();
    if (db_.changes() == 0) throw NotFoundError("event " + describeId(id) + " not found");
    tx.commit();
    return event;
}

void EventRepository::deleteEvent(const Id& id) {
    requireValidId(id, "event id");
    SQLiteTransaction tx(db_);
    SQLiteStatement del = db_.prepare("DELETE FROM events WHERE user_id = ? AND id_type = ? AND id_value = ?");
    del.bindInt(1, userId_);
    bindId(del, 2, id);
    del.execute();
    if (db_.changes() == 0) throw NotFoundError("event " + describeId(id) + " not found");
    tx.commit();
}

}  // namespace chronoplan::repositories
