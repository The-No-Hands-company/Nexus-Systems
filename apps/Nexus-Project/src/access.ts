import type { Database } from "bun:sqlite";
import { forbidden, notFound } from "./http";
import type { ProjectRow, Role, TaskRow } from "./store/rows";

export type { Role } from "./store/rows";

export const ROLES: readonly Role[] = ["viewer", "member", "admin", "owner"];
const RANK: Record<Role, number> = { viewer: 0, member: 1, admin: 2, owner: 3 };

export function atLeast(role: Role, minimum: Role): boolean {
  return RANK[role] >= RANK[minimum];
}

/** Seen but not permitted: 403. (Not seen at all is decided earlier, as 404.) */
export function requireRole(role: Role, minimum: Role): void {
  if (!atLeast(role, minimum)) throw forbidden();
}

export function workspaceRole(db: Database, workspaceId: string, subject: string): Role | null {
  const row = db
    .query("SELECT role FROM workspace_members WHERE workspace_id = ? AND subject = ?")
    .get(workspaceId, subject) as { role: Role } | null;
  return row ? row.role : null;
}

/** The caller's role in a workspace, or 404 so non-members cannot probe ids. */
export function workspaceAccess(db: Database, workspaceId: string, subject: string): Role {
  const role = workspaceRole(db, workspaceId, subject);
  if (!role) throw notFound("workspace");
  return role;
}

export interface ProjectAccess {
  project: ProjectRow;
  role: Role;
}

export function canSeeProject(db: Database, project: ProjectRow, subject: string, role: Role | null): boolean {
  if (!role) return false;
  if (project.visibility === "workspace" || atLeast(role, "admin")) return true;
  return Boolean(
    db.query("SELECT 1 FROM project_members WHERE project_id = ? AND subject = ?").get(project.id, subject),
  );
}

export function projectAccess(db: Database, projectId: string, subject: string): ProjectAccess {
  const project = db.query("SELECT * FROM projects WHERE id = ?").get(projectId) as ProjectRow | null;
  if (!project) throw notFound("project");
  const role = workspaceRole(db, project.workspace_id, subject);
  if (!role || !canSeeProject(db, project, subject, role)) throw notFound("project");
  return { project, role };
}

export interface TaskAccess extends ProjectAccess {
  task: TaskRow;
}

export function taskAccess(db: Database, taskId: string, subject: string): TaskAccess {
  const task = db.query("SELECT * FROM tasks WHERE id = ?").get(taskId) as TaskRow | null;
  if (!task) throw notFound("task");
  const project = db.query("SELECT * FROM projects WHERE id = ?").get(task.project_id) as ProjectRow;
  const role = workspaceRole(db, project.workspace_id, subject);
  if (!role || !canSeeProject(db, project, subject, role)) throw notFound("task");
  return { project, role, task };
}
