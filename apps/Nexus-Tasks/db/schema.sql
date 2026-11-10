-- Nexus Tasks — Minimalist Checklist & Daily Punchlists Schema
CREATE TABLE IF NOT EXISTS task_lists (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT '',
  task_count INTEGER DEFAULT 0,
  created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS tasks (
  id TEXT PRIMARY KEY,
  title TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT '',
  list_id TEXT NOT NULL,
  assignee TEXT DEFAULT '',
  priority TEXT DEFAULT 'medium',
  due_date TEXT DEFAULT '',
  completed INTEGER DEFAULT 0,
  created_at TEXT NOT NULL,
  FOREIGN KEY (list_id) REFERENCES task_lists (id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_tasks_list_id ON tasks (list_id);
CREATE INDEX IF NOT EXISTS idx_tasks_priority ON tasks (priority);
CREATE INDEX IF NOT EXISTS idx_tasks_completed ON tasks (completed);
