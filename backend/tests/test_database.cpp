#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include "TestDb.hpp"
#include "database/Migrations.hpp"
#include "database/SQLiteStatement.hpp"
#include "database/SQLiteTransaction.hpp"

using namespace chronoplan::database;
using testutil::TempDb;

namespace {
long long countRows(Database& db, const std::string& table) {
    SQLiteStatement s = db.prepare("SELECT COUNT(*) FROM " + table);  // table name is a test literal
    s.step();
    return s.columnInt64(0);
}
// One shared test account (id 1) per database; tasks must belong to a user.
void ensureUser(Database& db) {
    SQLiteStatement s = db.prepare(
        "INSERT OR IGNORE INTO users (id, email, password_hash, created_at) VALUES (1, 'u@example.com', 'x', 0)");
    s.execute();
}
void insertTask(Database& db, const std::string& type, const std::string& value) {
    ensureUser(db);
    SQLiteStatement s = db.prepare(
        "INSERT INTO tasks (user_id, id_type, id_value, title, duration_min, min_chunk_min) VALUES (1, ?, ?, 'T', 60, 30)");
    s.bindText(1, type).bindText(2, value);
    s.execute();
}
}  // namespace

TEST(Database, InitializationCreatesFileDirectoryAndSchema) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    EXPECT_TRUE(std::filesystem::exists(tmp.path()));
    EXPECT_EQ(mgr.schemaVersion(), latestSchemaVersion());
    for (const char* t : {"tasks", "events", "task_dependencies", "users", "sessions"}) {
        SQLiteStatement s = mgr.database().prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");
        s.bindText(1, t);
        EXPECT_TRUE(s.step()) << t;
    }
}

TEST(Database, ForeignKeysAreEnforced) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    Database& db = mgr.database();
    SQLiteStatement pragma = db.prepare("PRAGMA foreign_keys");
    ASSERT_TRUE(pragma.step());
    EXPECT_EQ(pragma.columnInt(0), 1);

    SQLiteStatement dep = db.prepare(
        "INSERT INTO task_dependencies VALUES (1, 'number', '1', 'number', '2')");
    try { dep.execute(); FAIL() << "expected a foreign key violation"; }
    catch (const DatabaseError& e) { EXPECT_TRUE(e.isConstraint()); }
}

TEST(Database, SchemaRejectsSelfAndDuplicateDependencies) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    Database& db = mgr.database();
    insertTask(db, "number", "1");
    insertTask(db, "number", "2");
    EXPECT_THROW(db.exec("INSERT INTO task_dependencies VALUES (1,'number','1','number','1')"), DatabaseError);
    db.exec("INSERT INTO task_dependencies VALUES (1,'number','1','number','2')");
    EXPECT_THROW(db.exec("INSERT INTO task_dependencies VALUES (1,'number','1','number','2')"), DatabaseError);
    // same value, different type is a different task, so this is NOT a self-dependency
    insertTask(db, "string", "1");
    EXPECT_NO_THROW(db.exec("INSERT INTO task_dependencies VALUES (1,'number','1','string','1')"));
}

TEST(Database, NumericAndStringIdsAreDistinctKeysInSchema) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    insertTask(mgr.database(), "number", "7");
    EXPECT_NO_THROW(insertTask(mgr.database(), "string", "7"));
    EXPECT_THROW(insertTask(mgr.database(), "number", "7"), DatabaseError);
    EXPECT_THROW(insertTask(mgr.database(), "float", "7"), DatabaseError);   // CHECK on id_type
}

TEST(Database, ConnectionFailureThrowsDatabaseError) {
    TempDb tmp;
    std::filesystem::create_directories(tmp.path());   // the "file" path is a directory
    EXPECT_THROW(DatabaseManager{tmp.config()}, DatabaseError);

    DatabaseConfig empty;
    empty.path = "";
    EXPECT_THROW(Database{empty}, DatabaseError);
}

TEST(Database, UnusableDirectoryThrows) {
    TempDb tmp;
    std::filesystem::create_directories(std::filesystem::path(tmp.path()).parent_path().parent_path());
    std::ofstream(std::filesystem::path(tmp.path()).parent_path().string()) << "x";  // 'nested' is a file
    EXPECT_THROW(DatabaseManager{tmp.config()}, DatabaseError);
}

TEST(Database, NonDatabaseFileIsRejected) {
    TempDb tmp;
    std::filesystem::create_directories(std::filesystem::path(tmp.path()).parent_path());
    std::ofstream(tmp.path()) << "this is definitely not a sqlite database, just text padding.........";
    EXPECT_THROW(DatabaseManager{tmp.config()}, DatabaseError);
}

TEST(Database, ErrorsDoNotLeakSqlDetails) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    try { mgr.database().exec("SELECT * FROM no_such_table"); FAIL(); }
    catch (const DatabaseError& e) { EXPECT_EQ(std::string(e.what()), "database operation failed"); }
}

TEST(Database, MigrationsAreIdempotentAndDataSurvivesReopen) {
    TempDb tmp;
    {
        DatabaseManager mgr(tmp.config());
        insertTask(mgr.database(), "string", "persist");
    }
    DatabaseManager again(tmp.config());   // reopen: must not recreate or wipe
    EXPECT_EQ(again.schemaVersion(), latestSchemaVersion());
    EXPECT_EQ(countRows(again.database(), "tasks"), 1);
    EXPECT_NO_THROW(applyMigrations(again.database()));
    EXPECT_EQ(countRows(again.database(), "tasks"), 1);
}

TEST(Database, NewerSchemaVersionIsRefused) {
    TempDb tmp;
    {
        DatabaseManager mgr(tmp.config());
        mgr.database().exec("PRAGMA user_version = 999");
    }
    EXPECT_THROW(DatabaseManager{tmp.config()}, DatabaseError);
}

TEST(Statement, BindsValuesWithoutInterpretingSql) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    Database& db = mgr.database();
    const std::string evil = "x'); DROP TABLE tasks; --";
    ensureUser(db);
    SQLiteStatement s = db.prepare(
        "INSERT INTO tasks (user_id, id_type, id_value, title, description, duration_min, min_chunk_min) "
        "VALUES (1, 'string', 'a', ?, ?, 60, 30)");
    s.bindText(1, evil).bindNull(2);
    s.execute();
    SQLiteStatement q = db.prepare("SELECT title, description FROM tasks");
    ASSERT_TRUE(q.step());
    EXPECT_EQ(q.columnText(0), evil);
    EXPECT_TRUE(q.columnIsNull(1));
    EXPECT_FALSE(q.columnOptText(1).has_value());
}

TEST(Statement, CanBeResetAndReused) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    Database& db = mgr.database();
    ensureUser(db);
    SQLiteStatement s = db.prepare(
        "INSERT INTO tasks (user_id, id_type, id_value, title, duration_min, min_chunk_min) VALUES (1, 'number', ?, 'T', 60, 30)");
    for (int i = 1; i <= 3; ++i) { s.bindText(1, std::to_string(i)); s.execute(); s.reset(); }
    EXPECT_EQ(countRows(db, "tasks"), 3);
}

TEST(Statement, InvalidSqlThrows) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    EXPECT_THROW(mgr.database().prepare("SELEKT nonsense"), DatabaseError);
}

TEST(Transaction, UncommittedWorkIsRolledBack) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    Database& db = mgr.database();
    {
        SQLiteTransaction tx(db);
        insertTask(db, "number", "1");
        EXPECT_EQ(countRows(db, "tasks"), 1);
    }   // no commit
    EXPECT_EQ(countRows(db, "tasks"), 0);
    EXPECT_EQ(db.transactionDepth(), 0);
}

TEST(Transaction, CommitPersists) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    Database& db = mgr.database();
    { SQLiteTransaction tx(db); insertTask(db, "number", "1"); tx.commit(); }
    EXPECT_EQ(countRows(db, "tasks"), 1);
}

TEST(Transaction, ExceptionRollsBackEverything) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    Database& db = mgr.database();
    try {
        SQLiteTransaction tx(db);
        insertTask(db, "number", "1");
        insertTask(db, "number", "1");   // duplicate -> throws
        tx.commit();
    } catch (const DatabaseError&) {}
    EXPECT_EQ(countRows(db, "tasks"), 0);
}

TEST(Transaction, NestedRollbackOnlyUndoesInnerWork) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    Database& db = mgr.database();
    {
        SQLiteTransaction outer(db);
        insertTask(db, "number", "1");
        { SQLiteTransaction inner(db); insertTask(db, "number", "2"); }   // rolled back
        { SQLiteTransaction inner(db); insertTask(db, "number", "3"); inner.commit(); }
        outer.commit();
    }
    EXPECT_EQ(countRows(db, "tasks"), 2);
    SQLiteStatement s = db.prepare("SELECT id_value FROM tasks ORDER BY id_value");
    ASSERT_TRUE(s.step()); EXPECT_EQ(s.columnText(0), "1");
    ASSERT_TRUE(s.step()); EXPECT_EQ(s.columnText(0), "3");
}

TEST(Config, EnvironmentOverridesPath) {
#ifdef _WIN32
    _putenv_s("CHRONOPLAN_DB_PATH", "custom/path.db");
#else
    setenv("CHRONOPLAN_DB_PATH", "custom/path.db", 1);
#endif
    EXPECT_EQ(DatabaseConfig::fromEnvironment().path, "custom/path.db");
#ifdef _WIN32
    _putenv_s("CHRONOPLAN_DB_PATH", "");
#else
    unsetenv("CHRONOPLAN_DB_PATH");
#endif
    EXPECT_EQ(DatabaseConfig::fromEnvironment().path, "./data/chronoplan.db");
}

// ---- Migration 002: existing data must survive and be owned by the locked legacy user ----
TEST(Migration, UpgradingAV1DatabasePreservesDataUnderTheLegacyOwner) {
    TempDb tmp;
    std::filesystem::create_directories(std::filesystem::path(tmp.path()).parent_path());
    {
        Database raw(tmp.config());   // a database exactly as the pre-authentication version left it
        raw.exec(allMigrations().front().sql);
        raw.exec("PRAGMA user_version = 1");
        raw.exec("INSERT INTO tasks (id_type, id_value, title, duration_min, min_chunk_min) VALUES "
                 "('number','1','old A',60,30), ('number','2','old B',30,30)");
        raw.exec("INSERT INTO events (id_type, id_value, title, date, start_min, end_min) VALUES "
                 "('number','1','old event','2026-11-02',540,600)");
        raw.exec("INSERT INTO task_dependencies VALUES ('number','2','number','1')");
    }
    DatabaseManager mgr(tmp.config());   // runs migration 2
    Database& db = mgr.database();
    EXPECT_EQ(mgr.schemaVersion(), 2);
    EXPECT_EQ(countRows(db, "tasks"), 2);
    EXPECT_EQ(countRows(db, "events"), 1);
    EXPECT_EQ(countRows(db, "task_dependencies"), 1);
    EXPECT_EQ(countRows(db, "users"), 1);
    SQLiteStatement s = db.prepare(
        "SELECT COUNT(*) FROM tasks t JOIN users u ON u.id = t.user_id WHERE u.is_system = 1 AND u.password_hash IS NULL");
    s.step();
    EXPECT_EQ(s.columnInt64(0), 2);   // owned by the system user that nobody can sign in as
}

TEST(Migration, AFreshDatabaseGetsNoLegacyUser) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    EXPECT_EQ(countRows(mgr.database(), "users"), 0);
}

TEST(Migration, DependencyEdgesCannotSpanUsers) {
    TempDb tmp;
    DatabaseManager mgr(tmp.config());
    Database& db = mgr.database();
    db.exec("INSERT INTO users (id, email, password_hash, created_at) VALUES (1,'a@x.io','h',0),(2,'b@x.io','h',0)");
    db.exec("INSERT INTO tasks (user_id,id_type,id_value,title,duration_min,min_chunk_min) VALUES "
            "(1,'number','1','a',60,30),(2,'number','2','b',60,30)");
    EXPECT_THROW(db.exec("INSERT INTO task_dependencies VALUES (1,'number','1','number','2')"), DatabaseError);
}
