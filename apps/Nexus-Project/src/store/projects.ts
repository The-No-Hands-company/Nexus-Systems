import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { canSeeProject, projectAccess, requireRole, workspaceAccess, workspaceRole } from "../access";
import { conflict, notFound, unprocessable } from "../http";
import { now, transaction } from "./db";
import type { ProjectRow, Role, StatusCategory } from "./rows";
import { reschedule } from "./scheduling";

export interface Project {
  id: string;
  workspaceId: string;
  key: string;
  name: string;
  description: string;
  startDate: string;
  scheduleMode: "manual" | "auto";
  visibility: "workspace" | "restricted";
  archived: boolean;
  role: Role;
  createdAt: string;
  updatedAt: string;
}

export interface ProjectInput {
  key: string;
  name: string;
  description: string;
  startDate: string;
  scheduleMode: "manual" | "auto";
  visibility: "workspace" | "restricted";
}

export interface ProjectPatch {
  name?: string;
  description?: string;
  startDate?: string;
  scheduleMode?: "manual" | "auto";
  visibility?: "workspace" | "restricted";
  archived?: boolean;
}

export interface ProjectMember {
  subject: string;
  createdAt: string;
}

export const DEFAULT_STATUSES: readonly (readonly [string, StatusCategory])[] = [
  ["Backlog", "backlog"],
  ["Todo", "unstarted"],
  ["In Progress", "started"],
  ["Done", "completed"],
  ["Canceled", "canceled"],
];

export function toProject(row: ProjectRow, role: Role): Project {
  return {
    id: row.id,
    workspaceId: row.workspace_id,
    key: row.key,
    name: row.name,
    description: row.description,
    startDate: row.start_date,
    scheduleMode: row.schedule_mode,
    visibility: row.visibility,
    archived: row.archived === 1,
    role,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
  };
}

function projectRow(db: Database, id: string): ProjectRow {
  return db.query("SELECT * FROM projects WHERE id = ?").get(id) as ProjectRow;
}

function assertKeyFree(db: Database, workspaceId: string, key: string): void {
  if (db.query("SELECT 1 FROM projects WHERE workspace_id = ? AND key = ?").get(workspaceId, key)) {
    throw conflict("key_taken", `project key ${key} is already used in this workspace`);
  }
}

export function createProject(db: Database, subject: string, workspaceId: string, input: ProjectInput): Project {
  const role = workspaceAccess(db, workspaceId, subject);
  requireRole(role, "member");
  return transaction(db, () => {
    assertKeyFree(db, workspaceId, input.key);
    const id = randomUUID();
    const at = now();
    db.query(
      `INSERT INTO projects (id, workspace_id, key, name, description, start_date, schedule_mode, visibility,
         archived, next_number, working_weekdays, created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, 1, 62, ?, ?, ?, ?)`,
    ).run(id, workspaceId, input.key, input.name, input.description, input.startDate, input.scheduleMode, input.visibility, at, at, subject, subject);
    const insertStatus = db.query(
      `INSERT INTO statuses (id, project_id, name, category, position, created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
    );
    DEFAULT_STATUSES.forEach(([name, category], position) => {
      insertStatus.run(randomUUID(), id, name, category, position, at, at, subject, subject);
    });
    // A member who creates a restricted project must still be able to see it.
    if (input.visibility === "restricted") {
      db.query("INSERT INTO project_members (project_id, subject, created_at, created_by) VALUES (?, ?, ?, ?)").run(id, subject, at, subject);
    }
    return toProject(projectRow(db, id), role);
  });
}

export function listProjects(db: Database, subject: string, workspaceId: string, includeArchived: boolean): Project[] {
  const role = workspaceAccess(db, workspaceId, subject);
  const rows = db
    .query(`SELECT * FROM projects WHERE workspace_id = ? AND (? = 1 OR archived = 0) ORDER BY name COLLATE NOCASE, id`)
    .all(workspaceId, includeArchived ? 1 : 0) as ProjectRow[];
  return rows.filter((row) => canSeeProject(db, row, subject, role)).map((row) => toProject(row, role));
}

export function getProject(db: Database, subject: string, id: string): Project {
  const { project, role } = projectAccess(db, id, subject);
  return toProject(project, role);
}

export function updateProject(db: Database, subject: string, id: string, patch: ProjectPatch): Project {
  const { project, role } = projectAccess(db, id, subject);
  requireRole(role, "admin");
  return transaction(db, () => {
    db.query(
      `UPDATE projects SET name = ?, description = ?, start_date = ?, schedule_mode = ?, visibility = ?, archived = ?,
         updated_at = ?, updated_by = ? WHERE id = ?`,
    ).run(
      patch.name ?? project.name,
      patch.description ?? project.description,
      patch.startDate ?? project.start_date,
      patch.scheduleMode ?? project.schedule_mode,
      patch.visibility ?? project.visibility,
      patch.archived === undefined ? project.archived : patch.archived ? 1 : 0,
      now(),
      subject,
      id,
    );
    if (patch.startDate !== undefined || patch.scheduleMode !== undefined) reschedule(db, id, subject);
    return toProject(projectRow(db, id), role);
  });
}

export function moveProject(db: Database, subject: string, id: string, targetWorkspaceId: string): Project {
  const { project, role } = projectAccess(db, id, subject);
  requireRole(role, "admin");
  const targetRole = workspaceAccess(db, targetWorkspaceId, subject);
  requireRole(targetRole, "admin");
  if (targetWorkspaceId === project.workspace_id) return toProject(project, role);
  return transaction(db, () => {
    assertKeyFree(db, targetWorkspaceId, project.key);
    db.query("UPDATE projects SET workspace_id = ?, updated_at = ?, updated_by = ? WHERE id = ?").run(targetWorkspaceId, now(), subject, id);
    db.query(
      `DELETE FROM project_members WHERE project_id = ?
         AND subject NOT IN (SELECT subject FROM workspace_members WHERE workspace_id = ?)`,
    ).run(id, targetWorkspaceId);
    return toProject(projectRow(db, id), targetRole);
  });
}

export function listProjectMembers(db: Database, subject: string, id: string): ProjectMember[] {
  projectAccess(db, id, subject);
  const rows = db.query("SELECT subject, created_at FROM project_members WHERE project_id = ? ORDER BY subject").all(id) as {
    subject: string;
    created_at: string;
  }[];
  return rows.map((row) => ({ subject: row.subject, createdAt: row.created_at }));
}

export function addProjectMember(db: Database, subject: string, id: string, target: string): ProjectMember {
  const { project, role } = projectAccess(db, id, subject);
  requireRole(role, "admin");
  if (!workspaceRole(db, project.workspace_id, target)) {
    throw unprocessable("not_workspace_member", "only members of the project's workspace can be listed on it");
  }
  const at = now();
  db.query("INSERT OR IGNORE INTO project_members (project_id, subject, created_at, created_by) VALUES (?, ?, ?, ?)").run(id, target, at, subject);
  const row = db.query("SELECT subject, created_at FROM project_members WHERE project_id = ? AND subject = ?").get(id, target) as {
    subject: string;
    created_at: string;
  };
  return { subject: row.subject, createdAt: row.created_at };
}

export function removeProjectMember(db: Database, subject: string, id: string, target: string): void {
  const { role } = projectAccess(db, id, subject);
  requireRole(role, "admin");
  const removed = db.query("DELETE FROM project_members WHERE project_id = ? AND subject = ?").run(id, target);
  if (removed.changes === 0) throw notFound("project member");
}
