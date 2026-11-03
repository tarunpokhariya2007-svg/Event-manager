#include "auth/SessionRepository.hpp"
#include "database/SQLiteStatement.hpp"
#include "database/SQLiteTransaction.hpp"

namespace chronoplan::auth {

using database::SQLiteStatement;
using database::SQLiteTransaction;

void SessionRepository::create(repositories::UserId userId, const std::string& tokenHash, long long now,
                               long long expiresAt) {
    SQLiteTransaction tx(db_);
    SQLiteStatement s = db_.prepare(
        "INSERT INTO sessions (user_id, token_hash, created_at, last_seen_at, expires_at) VALUES (?, ?, ?, ?, ?)");
    s.bindInt(1, userId).bindText(2, tokenHash).bindInt(3, now).bindInt(4, now).bindInt(5, expiresAt);
    s.execute();
    tx.commit();
}

std::optional<Session> SessionRepository::findByTokenHash(const std::string& tokenHash) {
    auto lock = db_.lock();
    SQLiteStatement s = db_.prepare(
        "SELECT id, user_id, token_hash, created_at, last_seen_at, expires_at FROM sessions WHERE token_hash = ?");
    s.bindText(1, tokenHash);
    if (!s.step()) return std::nullopt;
    Session out;
    out.id = s.columnInt64(0);
    out.userId = s.columnInt64(1);
    out.tokenHash = s.columnText(2);
    out.createdAt = s.columnInt64(3);
    out.lastSeenAt = s.columnInt64(4);
    out.expiresAt = s.columnInt64(5);
    return out;
}

void SessionRepository::touch(long long sessionId, long long now) {
    SQLiteTransaction tx(db_);
    SQLiteStatement s = db_.prepare("UPDATE sessions SET last_seen_at = ? WHERE id = ?");
    s.bindInt(1, now).bindInt(2, sessionId);
    s.execute();
    tx.commit();
}

void SessionRepository::deleteByTokenHash(const std::string& tokenHash) {
    SQLiteTransaction tx(db_);
    SQLiteStatement s = db_.prepare("DELETE FROM sessions WHERE token_hash = ?");
    s.bindText(1, tokenHash);
    s.execute();
    tx.commit();
}

void SessionRepository::deleteAllForUser(repositories::UserId userId) {
    SQLiteTransaction tx(db_);
    SQLiteStatement s = db_.prepare("DELETE FROM sessions WHERE user_id = ?");
    s.bindInt(1, userId);
    s.execute();
    tx.commit();
}

void SessionRepository::deleteStale(long long now, long long idleSeconds) {
    SQLiteTransaction tx(db_);
    SQLiteStatement s = db_.prepare("DELETE FROM sessions WHERE expires_at <= ? OR last_seen_at <= ?");
    s.bindInt(1, now).bindInt(2, now - idleSeconds);
    s.execute();
    tx.commit();
}

}  // namespace chronoplan::auth
