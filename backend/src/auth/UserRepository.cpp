#include "auth/UserRepository.hpp"
#include "auth/AuthErrors.hpp"
#include "database/SQLiteStatement.hpp"
#include "database/SQLiteTransaction.hpp"

namespace chronoplan::auth {

using database::SQLiteStatement;
using database::SQLiteTransaction;

namespace {

constexpr const char* kSelect =
    "SELECT id, email, password_hash, google_sub, display_name, email_verified, is_system, created_at FROM users";

User fromRow(const SQLiteStatement& s) {
    User u;
    u.id = s.columnInt64(0);
    u.email = s.columnText(1);
    u.passwordHash = s.columnOptText(2);
    u.googleSub = s.columnOptText(3);
    u.displayName = s.columnText(4);
    u.emailVerified = s.columnInt(5) != 0;
    u.isSystem = s.columnInt(6) != 0;
    u.createdAt = s.columnInt64(7);
    return u;
}

}  // namespace

User UserRepository::createPasswordUser(const std::string& email, const std::string& passwordHash,
                                        const std::string& displayName, long long now) {
    SQLiteTransaction tx(db_);
    SQLiteStatement ins = db_.prepare(
        "INSERT INTO users (email, password_hash, display_name, email_verified, created_at) VALUES (?, ?, ?, 0, ?)");
    ins.bindText(1, email).bindText(2, passwordHash).bindText(3, displayName).bindInt(4, now);
    try {
        ins.execute();
    } catch (const database::DatabaseError& e) {
        if (e.isConstraint()) throw EmailTakenError("an account with this e-mail address already exists");
        throw;
    }
    const long long id = sqlite3_last_insert_rowid(db_.handle());
    tx.commit();
    return *findById(id);
}

User UserRepository::createGoogleUser(const std::string& email, const std::string& googleSub,
                                      const std::string& displayName, long long now) {
    SQLiteTransaction tx(db_);
    SQLiteStatement ins = db_.prepare(
        "INSERT INTO users (email, google_sub, display_name, email_verified, created_at) VALUES (?, ?, ?, 1, ?)");
    ins.bindText(1, email).bindText(2, googleSub).bindText(3, displayName).bindInt(4, now);
    try {
        ins.execute();
    } catch (const database::DatabaseError& e) {
        if (e.isConstraint()) throw EmailTakenError("an account with this e-mail address already exists");
        throw;
    }
    const long long id = sqlite3_last_insert_rowid(db_.handle());
    tx.commit();
    return *findById(id);
}

std::optional<User> UserRepository::findByEmail(const std::string& email) {
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(std::string(kSelect) + " WHERE email = ?");   // column is COLLATE NOCASE
    s.bindText(1, email);
    if (!s.step()) return std::nullopt;
    return fromRow(s);
}

std::optional<User> UserRepository::findById(repositories::UserId id) {
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(std::string(kSelect) + " WHERE id = ?");
    s.bindInt(1, id);
    if (!s.step()) return std::nullopt;
    return fromRow(s);
}

std::optional<User> UserRepository::findByGoogleSub(const std::string& sub) {
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(std::string(kSelect) + " WHERE google_sub = ?");
    s.bindText(1, sub);
    if (!s.step()) return std::nullopt;
    return fromRow(s);
}

void UserRepository::recordLogin(repositories::UserId id, long long now) {
    SQLiteTransaction tx(db_);
    SQLiteStatement s = db_.prepare("UPDATE users SET last_login_at = ? WHERE id = ?");
    s.bindInt(1, now).bindInt(2, id);
    s.execute();
    tx.commit();
}

void UserRepository::updatePasswordHash(repositories::UserId id, const std::string& passwordHash) {
    SQLiteTransaction tx(db_);
    SQLiteStatement s = db_.prepare("UPDATE users SET password_hash = ? WHERE id = ?");
    s.bindText(1, passwordHash).bindInt(2, id);
    s.execute();
    tx.commit();
}

}  // namespace chronoplan::auth
