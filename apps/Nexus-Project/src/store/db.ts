import { Database } from "bun:sqlite";

const AUDIT = `created_at TEXT NOT NULL, updated_at TEXT NOT NULL,
  created_by TEXT NOT NULL, updated_by TEXT NOT NULL`;

/** Index i migrates schema version i to i + 1. Append only; never edit a shipped entry. */
const MIGRATIONS: readonly string[] = [
  `
  CREATE TABLE workspaces (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    kind TEXT NOT NULL CHECK (kind IN ('personal', 'team')),
    personal_subject TEXT UNIQUE,
    ${AUDIT},
    CHECK ((kind = 'personal') = (personal_subject IS NOT NULL))
  );
  CREATE TABLE workspace_members (
    workspace_id TEXT NOT NULL REFERENCES workspaces(id) ON DELETE CASCADE,
    subject TEXT NOT NULL,
    role TEXT NOT NULL CHECK (role IN ('owner', 'admin', 'member', 'viewer')),
    ${AUDIT},
    PRIMARY KEY (workspace_id, subject)
  );
  CREATE INDEX workspace_members_subject ON workspace_members (subject);
  CREATE TABLE projects (
    id TEXT PRIMARY KEY,
    workspace_id TEXT NOT NULL REFERENCES workspaces(id) ON DELETE CASCADE,
    key TEXT NOT NULL,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    start_date TEXT NOT NULL,
    schedule_mode TEXT NOT NULL DEFAULT 'manual' CHECK (schedule_mode IN ('manual', 'auto')),
    visibility TEXT NOT NULL DEFAULT 'workspace' CHECK (visibility IN ('workspace', 'restricted')),
    archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),
    next_number INTEGER NOT NULL DEFAULT 1,
    working_weekdays INTEGER NOT NULL DEFAULT 62 CHECK (working_weekdays BETWEEN 1 AND 127),
    ${AUDIT},
    UNIQUE (workspace_id, key)
  );
  CREATE TABLE project_members (
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    subject TEXT NOT NULL,
    created_at TEXT NOT NULL,
    created_by TEXT NOT NULL,
    PRIMARY KEY (project_id, subject)
  );
  CREATE TABLE calendar_exceptions (
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    date TEXT NOT NULL,
    working INTEGER NOT NULL CHECK (working IN (0, 1)),
    PRIMARY KEY (project_id, date)
  );
  CREATE TABLE statuses (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    category TEXT NOT NULL CHECK (category IN ('backlog', 'unstarted', 'started', 'completed', 'canceled')),
    position INTEGER NOT NULL,
    ${AUDIT},
    UNIQUE (project_id, name)
  );
  CREATE TABLE tasks (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    number INTEGER NOT NULL,
    parent_id TEXT REFERENCES tasks(id) ON DELETE CASCADE,
    rank TEXT NOT NULL,
    title TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status_id TEXT NOT NULL REFERENCES statuses(id),
    priority TEXT NOT NULL DEFAULT 'none' CHECK (priority IN ('none', 'low', 'medium', 'high', 'urgent')),
    assignee_subject TEXT,
    kind TEXT NOT NULL DEFAULT 'task' CHECK (kind IN ('task', 'milestone')),
    duration_days INTEGER CHECK (duration_days IS NULL OR duration_days >= 0),
    start_date TEXT,
    finish_date TEXT,
    constraint_type TEXT NOT NULL DEFAULT 'asap' CHECK (constraint_type IN ('asap', 'start_no_earlier_than')),
    constraint_date TEXT,
    deadline TEXT,
    progress INTEGER NOT NULL DEFAULT 0 CHECK (progress BETWEEN 0 AND 100),
    version INTEGER NOT NULL DEFAULT 1,
    ${AUDIT},
    UNIQUE (project_id, number)
  );
  CREATE INDEX tasks_project_rank ON tasks (project_id, rank);
  CREATE INDEX tasks_parent ON tasks (parent_id);
  CREATE INDEX tasks_assignee ON tasks (assignee_subject);
  CREATE TABLE dependencies (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    predecessor_id TEXT NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
    successor_id TEXT NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
    type TEXT NOT NULL CHECK (type IN ('FS', 'SS', 'FF', 'SF')),
    lag_days INTEGER NOT NULL DEFAULT 0,
    ${AUDIT},
    UNIQUE (predecessor_id, successor_id),
    CHECK (predecessor_id <> successor_id)
  );
  CREATE INDEX dependencies_project ON dependencies (project_id);
  CREATE INDEX dependencies_successor ON dependencies (successor_id);
  `,
];

export function openDatabase(path: string): Database {
  const db = new Database(path, { create: true });
  db.exec("PRAGMA foreign_keys = ON");
  if (path !== ":memory:") db.exec("PRAGMA journal_mode = WAL");
  migrate(db);
  return db;
}

function migrate(db: Database): void {
  const current = (db.query("PRAGMA user_version").get() as { user_version: number }).user_version;
  for (let version = current; version < MIGRATIONS.length; version++) {
    // One transaction per step: a half-applied schema is not recoverable.
    db.exec("BEGIN IMMEDIATE");
    try {
      db.exec(MIGRATIONS[version] as string);
      db.exec(`PRAGMA user_version = ${version + 1}`);
      db.exec("COMMIT");
    } catch (error) {
      db.exec("ROLLBACK");
      throw error;
    }
  }
}

/** Runs `fn` atomically. Nested calls become savepoints. A throw rolls back. */
export function transaction<T>(db: Database, fn: () => T): T {
  return db.transaction(fn).immediate();
}

export function now(): string {
  return new Date().toISOString();
}
