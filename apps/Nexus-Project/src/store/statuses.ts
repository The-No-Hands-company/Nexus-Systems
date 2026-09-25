import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { type Role, projectAccess, requireRole } from "../access";
import { badRequest, conflict, notFound, unprocessable } from "../http";
import { now, transaction } from "./db";
import type { ProjectRow, StatusCategory, StatusRow } from "./rows";
import { reschedule } from "./scheduling";

export const STATUS_CATEGORIES: readonly StatusCategory[] = [
  "backlog",
  "unstarted",
  "started",
  "completed",
  "canceled",
];

export interface Status {
  id: string;
  projectId: string;
  name: string;
  category: StatusCategory;
  position: number;
}

export interface StatusPatch {
  name?: string;
  category?: StatusCategory;
  position?: number;
}

function toStatus(row: StatusRow): Status {
  return {
    id: row.id,
    projectId: row.project_id,
    name: row.name,
    category: row.category,
    position: row.position,
  };
}

function rows(db: Database, projectId: string): StatusRow[] {
  return db
    .query("SELECT * FROM statuses WHERE project_id = ? ORDER BY position, id")
    .all(projectId) as StatusRow[];
}

function statusAccess(
  db: Database,
  statusId: string,
  subject: string,
): { status: StatusRow; project: ProjectRow; role: Role } {
  const status = db.query("SELECT * FROM statuses WHERE id = ?").get(statusId) as StatusRow | null;
  if (!status) throw notFound("status");
  try {
    return { status, ...projectAccess(db, status.project_id, subject) };
  } catch {
    throw notFound("status");
  }
}

function renumber(db: Database, ordered: readonly string[]): void {
  const update = db.query("UPDATE statuses SET position = ? WHERE id = ?");
  ordered.forEach((id, position) => update.run(position, id));
}

function assertNameFree(
  db: Database,
  projectId: string,
  name: string,
  except: string | null,
): void {
  const clash = db
    .query("SELECT id FROM statuses WHERE project_id = ? AND name = ?")
    .get(projectId, name) as { id: string } | null;
  if (clash && clash.id !== except)
    throw conflict("status_name_taken", `a status named ${name} already exists`);
}

export function listStatuses(db: Database, subject: string, projectId: string): Status[] {
  projectAccess(db, projectId, subject);
  return rows(db, projectId).map(toStatus);
}

export function defaultStatusId(db: Database, projectId: string): string {
  const all = rows(db, projectId);
  const chosen = all.find((s) => s.category === "unstarted") ?? all[0];
  if (!chosen) throw new Error(`project ${projectId} has no statuses`);
  return chosen.id;
}

export function createStatus(
  db: Database,
  subject: string,
  projectId: string,
  name: string,
  category: StatusCategory,
): Status {
  const { role } = projectAccess(db, projectId, subject);
  requireRole(role, "admin");
  return transaction(db, () => {
    assertNameFree(db, projectId, name, null);
    const id = randomUUID();
    const at = now();
    db.query(
      `INSERT INTO statuses (id, project_id, name, category, position, created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
    ).run(id, projectId, name, category, rows(db, projectId).length, at, at, subject, subject);
    return toStatus(db.query("SELECT * FROM statuses WHERE id = ?").get(id) as StatusRow);
  });
}

export function updateStatus(
  db: Database,
  subject: string,
  statusId: string,
  patch: StatusPatch,
): Status {
  const { status, role } = statusAccess(db, statusId, subject);
  requireRole(role, "admin");
  return transaction(db, () => {
    if (patch.name !== undefined) assertNameFree(db, status.project_id, patch.name, status.id);
    db.query(
      "UPDATE statuses SET name = ?, category = ?, updated_at = ?, updated_by = ? WHERE id = ?",
    ).run(patch.name ?? status.name, patch.category ?? status.category, now(), subject, status.id);
    if (patch.position !== undefined) {
      const others = rows(db, status.project_id)
        .map((s) => s.id)
        .filter((id) => id !== status.id);
      if (patch.position > others.length)
        throw badRequest(`position must be from 0 to ${others.length}`);
      others.splice(patch.position, 0, status.id);
      renumber(db, others);
    }
    // A category decides whether tasks are open, pinned as completed or left out.
    if (patch.category !== undefined && patch.category !== status.category)
      reschedule(db, status.project_id, subject);
    return toStatus(db.query("SELECT * FROM statuses WHERE id = ?").get(status.id) as StatusRow);
  });
}

export function deleteStatus(
  db: Database,
  subject: string,
  statusId: string,
  moveTasksTo: string | null,
): void {
  const { status, role } = statusAccess(db, statusId, subject);
  requireRole(role, "admin");
  transaction(db, () => {
    const all = rows(db, status.project_id);
    if (all.length === 1) throw unprocessable("last_status", "a project needs at least one status");
    if (
      moveTasksTo !== null &&
      (moveTasksTo === status.id || !all.some((s) => s.id === moveTasksTo))
    ) {
      throw badRequest("moveTasksTo must be another status of the same project");
    }
    const inUse = (
      db.query("SELECT COUNT(*) AS n FROM tasks WHERE status_id = ?").get(status.id) as {
        n: number;
      }
    ).n;
    if (inUse > 0) {
      if (moveTasksTo === null)
        throw unprocessable("status_in_use", `${inUse} task(s) use this status; pass moveTasksTo`);
      db.query(
        "UPDATE tasks SET status_id = ?, version = version + 1, updated_at = ?, updated_by = ? WHERE status_id = ?",
      ).run(moveTasksTo, now(), subject, status.id);
      reschedule(db, status.project_id, subject);
    }
    db.query("DELETE FROM statuses WHERE id = ?").run(status.id);
    renumber(
      db,
      all.map((s) => s.id).filter((id) => id !== status.id),
    );
  });
}
