-- ChronoPlan schema v2: accounts, sessions and per-user ownership of tasks/events.
-- Applied inside one transaction by applyMigrations (PRAGMA user_version moves 1 -> 2 only on success).
-- Existing rows are preserved: they are assigned to a locked "legacy" system user (nobody can log in
-- as it) so no registered user can see them until the operator explicitly moves them (see
-- backend/AUTH.md). tasks/events/task_dependencies are rebuilt because SQLite cannot change a
-- primary key in place. Ids stay unique per user, not globally.

CREATE TABLE users (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    email          TEXT    NOT NULL COLLATE NOCASE CHECK (length(email) BETWEEN 3 AND 254),
    password_hash  TEXT,                    -- Argon2id encoded string; NULL for Google-only accounts
    google_sub     TEXT,                    -- Google's stable subject id; NULL for password accounts
    display_name   TEXT    NOT NULL DEFAULT '',
    email_verified INTEGER NOT NULL DEFAULT 0 CHECK (email_verified IN (0, 1)),
    is_system      INTEGER NOT NULL DEFAULT 0 CHECK (is_system IN (0, 1)),
    created_at     INTEGER NOT NULL,        -- unix seconds
    last_login_at  INTEGER,
    CHECK (is_system = 1 OR password_hash IS NOT NULL OR google_sub IS NOT NULL)
);
CREATE UNIQUE INDEX idx_users_email ON users (email);
CREATE UNIQUE INDEX idx_users_google_sub ON users (google_sub) WHERE google_sub IS NOT NULL;

-- Only the SHA-256 hash of the cookie token is stored, never the token itself. (The CSRF token is
-- derived from the cookie token on each request, so it needs no column.)
CREATE TABLE sessions (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id      INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    token_hash   TEXT    NOT NULL UNIQUE,
    created_at   INTEGER NOT NULL,
    last_seen_at INTEGER NOT NULL,
    expires_at   INTEGER NOT NULL           -- absolute expiry (unix seconds)
);
CREATE INDEX idx_sessions_user ON sessions (user_id);
CREATE INDEX idx_sessions_expires ON sessions (expires_at);

-- Owner of data that existed before authentication. Created only when there is data to own.
INSERT INTO users (email, password_hash, google_sub, display_name, email_verified, is_system, created_at)
SELECT 'legacy-data@chronoplan.invalid', NULL, NULL, 'Legacy data (unclaimed)', 0, 1, CAST(strftime('%s', 'now') AS INTEGER)
WHERE EXISTS (SELECT 1 FROM tasks) OR EXISTS (SELECT 1 FROM events);

CREATE TABLE tasks_v2 (
    user_id               INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    id_type               TEXT    NOT NULL CHECK (id_type IN ('number', 'string')),
    id_value              TEXT    NOT NULL CHECK (length(id_value) BETWEEN 1 AND 64),
    title                 TEXT    NOT NULL CHECK (length(title) >= 1),
    description           TEXT,
    technical_context     TEXT,
    priority              INTEGER NOT NULL DEFAULT 3 CHECK (priority BETWEEN 1 AND 5),
    duration_min          INTEGER NOT NULL CHECK (duration_min BETWEEN 1 AND 1440),
    min_chunk_min         INTEGER NOT NULL CHECK (min_chunk_min >= 1 AND min_chunk_min <= duration_min),
    allow_split           INTEGER NOT NULL DEFAULT 1 CHECK (allow_split IN (0, 1)),
    deadline_date         TEXT    CHECK (deadline_date IS NULL OR length(deadline_date) = 10),
    deadline_min          INTEGER NOT NULL DEFAULT 1440 CHECK (deadline_min BETWEEN 0 AND 1440),
    earliest_start_min    INTEGER CHECK (earliest_start_min IS NULL OR earliest_start_min BETWEEN 0 AND 1439),
    latest_completion_min INTEGER CHECK (latest_completion_min IS NULL OR latest_completion_min BETWEEN 1 AND 1440),
    status                TEXT,
    PRIMARY KEY (user_id, id_type, id_value)
);

CREATE TABLE events_v2 (
    user_id     INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    id_type     TEXT    NOT NULL CHECK (id_type IN ('number', 'string')),
    id_value    TEXT    NOT NULL CHECK (length(id_value) BETWEEN 1 AND 64),
    title       TEXT    NOT NULL CHECK (length(title) >= 1),
    date        TEXT    NOT NULL CHECK (length(date) = 10),
    start_min   INTEGER NOT NULL CHECK (start_min BETWEEN 0 AND 1439),
    end_min     INTEGER NOT NULL CHECK (end_min BETWEEN 1 AND 1440),
    priority    INTEGER NOT NULL DEFAULT 3 CHECK (priority BETWEEN 1 AND 5),
    is_flexible INTEGER NOT NULL DEFAULT 0 CHECK (is_flexible IN (0, 1)),
    category    TEXT    NOT NULL DEFAULT '',
    location    TEXT    NOT NULL DEFAULT '',
    PRIMARY KEY (user_id, id_type, id_value),
    CHECK (start_min < end_min)
);

-- Both endpoints of an edge must belong to the same user (composite foreign keys).
CREATE TABLE task_dependencies_v2 (
    user_id               INTEGER NOT NULL,
    task_id_type          TEXT NOT NULL,
    task_id_value         TEXT NOT NULL,
    prerequisite_id_type  TEXT NOT NULL,
    prerequisite_id_value TEXT NOT NULL,
    PRIMARY KEY (user_id, task_id_type, task_id_value, prerequisite_id_type, prerequisite_id_value),
    CHECK (NOT (task_id_type = prerequisite_id_type AND task_id_value = prerequisite_id_value)),
    FOREIGN KEY (user_id, task_id_type, task_id_value)
        REFERENCES tasks_v2 (user_id, id_type, id_value) ON DELETE CASCADE,
    FOREIGN KEY (user_id, prerequisite_id_type, prerequisite_id_value)
        REFERENCES tasks_v2 (user_id, id_type, id_value) ON DELETE CASCADE
);

INSERT INTO tasks_v2 (user_id, id_type, id_value, title, description, technical_context, priority,
                      duration_min, min_chunk_min, allow_split, deadline_date, deadline_min,
                      earliest_start_min, latest_completion_min, status)
SELECT (SELECT id FROM users WHERE is_system = 1), id_type, id_value, title, description, technical_context,
       priority, duration_min, min_chunk_min, allow_split, deadline_date, deadline_min,
       earliest_start_min, latest_completion_min, status
FROM tasks ORDER BY rowid;

INSERT INTO events_v2 (user_id, id_type, id_value, title, date, start_min, end_min, priority,
                       is_flexible, category, location)
SELECT (SELECT id FROM users WHERE is_system = 1), id_type, id_value, title, date, start_min, end_min,
       priority, is_flexible, category, location
FROM events ORDER BY rowid;

INSERT INTO task_dependencies_v2 (user_id, task_id_type, task_id_value, prerequisite_id_type, prerequisite_id_value)
SELECT (SELECT id FROM users WHERE is_system = 1), task_id_type, task_id_value, prerequisite_id_type,
       prerequisite_id_value
FROM task_dependencies ORDER BY rowid;

-- Child table first, so dropping `tasks` cannot cascade into rows that were not copied yet.
DROP TABLE task_dependencies;
DROP TABLE tasks;
DROP TABLE events;
ALTER TABLE tasks_v2 RENAME TO tasks;
ALTER TABLE events_v2 RENAME TO events;
ALTER TABLE task_dependencies_v2 RENAME TO task_dependencies;

CREATE INDEX idx_events_user_date ON events (user_id, date, start_min);
CREATE INDEX idx_task_dependencies_prerequisite
    ON task_dependencies (user_id, prerequisite_id_type, prerequisite_id_value);
