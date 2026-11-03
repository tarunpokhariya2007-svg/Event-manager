-- ChronoPlan schema v1.
-- Identifiers are stored as a (type, value) pair so that numeric id 7 and
-- string id "7" are distinct keys:  id_type IN ('number','string'), id_value = canonical text.
-- Optional model fields are SQL NULL. Foreign keys require PRAGMA foreign_keys=ON (set per connection).

CREATE TABLE tasks (
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
    PRIMARY KEY (id_type, id_value)
);

CREATE TABLE events (
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
    PRIMARY KEY (id_type, id_value),
    CHECK (start_min < end_min)
);
CREATE INDEX idx_events_date ON events (date, start_min);

-- "task <task_id> requires <prerequisite_id> to be done first".
-- Single source of truth for Task.prerequisite_ids (never duplicated in tasks).
CREATE TABLE task_dependencies (
    task_id_type          TEXT NOT NULL,
    task_id_value         TEXT NOT NULL,
    prerequisite_id_type  TEXT NOT NULL,
    prerequisite_id_value TEXT NOT NULL,
    PRIMARY KEY (task_id_type, task_id_value, prerequisite_id_type, prerequisite_id_value),
    CHECK (NOT (task_id_type = prerequisite_id_type AND task_id_value = prerequisite_id_value)),
    FOREIGN KEY (task_id_type, task_id_value)
        REFERENCES tasks (id_type, id_value) ON DELETE CASCADE,
    FOREIGN KEY (prerequisite_id_type, prerequisite_id_value)
        REFERENCES tasks (id_type, id_value) ON DELETE CASCADE
);
CREATE INDEX idx_task_dependencies_prerequisite
    ON task_dependencies (prerequisite_id_type, prerequisite_id_value);
