import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { requireRole, taskAccess } from "../access";
import { badRequest } from "../http";
import { now } from "./db";

export type ActivityKind =
  | "comment"
  | "status_change"
  | "assignment"
  | "update"
  | "subtask"
  | "created";

export interface TaskActivity {
  id: string;
  taskId: string;
  projectId: string;
  kind: ActivityKind;
  authorSubject: string;
  message: string;
  metadata?: Record<string, unknown> | undefined;
  createdAt: string;
}

interface ActivityRow {
  id: string;
  task_id: string;
  project_id: string;
  kind: ActivityKind;
  author_subject: string;
  message: string;
  metadata: string;
  created_at: string;
}

function toActivity(row: ActivityRow): TaskActivity {
  let metadata: Record<string, unknown> | undefined;
  try {
    metadata = JSON.parse(row.metadata);
  } catch {
    metadata = undefined;
  }
  return {
    id: row.id,
    taskId: row.task_id,
    projectId: row.project_id,
    kind: row.kind,
    authorSubject: row.author_subject,
    message: row.message,
    ...(metadata ? { metadata } : {}),
    createdAt: row.created_at,
  };
}

export function logActivity(
  db: Database,
  taskId: string,
  projectId: string,
  authorSubject: string,
  kind: ActivityKind,
  message: string,
  metadata?: Record<string, unknown>,
): TaskActivity {
  const id = randomUUID();
  const createdAt = now();
  const metaJson = JSON.stringify(metadata ?? {});
  db.query(
    `INSERT INTO task_activities (id, task_id, project_id, kind, author_subject, message, metadata, created_at)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
  ).run(id, taskId, projectId, kind, authorSubject, message, metaJson, createdAt);

  return {
    id,
    taskId,
    projectId,
    kind,
    authorSubject,
    message,
    ...(metadata ? { metadata } : {}),
    createdAt,
  };
}

export function listActivities(db: Database, subject: string, taskId: string): TaskActivity[] {
  taskAccess(db, taskId, subject);
  const rows = db
    .query("SELECT * FROM task_activities WHERE task_id = ? ORDER BY created_at ASC")
    .all(taskId) as ActivityRow[];
  return rows.map(toActivity);
}

export function addComment(
  db: Database,
  subject: string,
  taskId: string,
  message: string,
): TaskActivity {
  const { role, project } = taskAccess(db, taskId, subject);
  requireRole(role, "member");
  const trimmed = message.trim();
  if (!trimmed) throw badRequest("comment message cannot be empty");
  return logActivity(db, taskId, project.id, subject, "comment", trimmed);
}
