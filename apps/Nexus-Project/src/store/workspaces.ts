import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { requireRole, workspaceAccess, workspaceRole } from "../access";
import { forbidden, notFound, unprocessable } from "../http";
import { now, transaction } from "./db";
import type { MemberRow, Role, WorkspaceRow } from "./rows";

export interface Workspace {
  id: string;
  name: string;
  kind: "personal" | "team";
  role: Role;
  createdAt: string;
  updatedAt: string;
}

export interface Member {
  subject: string;
  role: Role;
  createdAt: string;
  updatedAt: string;
}

function toWorkspace(row: WorkspaceRow, role: Role): Workspace {
  return {
    id: row.id,
    name: row.name,
    kind: row.kind,
    role,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
  };
}

function toMember(row: MemberRow): Member {
  return {
    subject: row.subject,
    role: row.role,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
  };
}

function workspaceRow(db: Database, id: string): WorkspaceRow {
  const row = db.query("SELECT * FROM workspaces WHERE id = ?").get(id) as WorkspaceRow | null;
  if (!row) throw notFound("workspace");
  return row;
}

function ownerCount(db: Database, id: string): number {
  return (
    db
      .query(
        "SELECT COUNT(*) AS n FROM workspace_members WHERE workspace_id = ? AND role = 'owner'",
      )
      .get(id) as { n: number }
  ).n;
}

function insertMember(
  db: Database,
  workspaceId: string,
  subject: string,
  role: Role,
  by: string,
): void {
  const at = now();
  db.query(
    `INSERT INTO workspace_members (workspace_id, subject, role, created_at, updated_at, created_by, updated_by)
     VALUES (?, ?, ?, ?, ?, ?, ?)
     ON CONFLICT (workspace_id, subject) DO UPDATE SET
       role = excluded.role, updated_at = excluded.updated_at, updated_by = excluded.updated_by`,
  ).run(workspaceId, subject, role, at, at, by, by);
}

const PERSONAL =
  "a personal workspace has exactly one member; move the project into a team workspace to collaborate";

/** Every subject gets a personal workspace on first use, so a solo user never sees setup. */
export function ensurePersonalWorkspace(db: Database, subject: string): void {
  if (db.query("SELECT 1 FROM workspaces WHERE personal_subject = ?").get(subject)) return;
  transaction(db, () => {
    const id = randomUUID();
    const at = now();
    // OR IGNORE: two first requests racing must still produce one workspace.
    const inserted = db
      .query(
        `INSERT OR IGNORE INTO workspaces (id, name, kind, personal_subject, created_at, updated_at, created_by, updated_by)
         VALUES (?, 'Personal', 'personal', ?, ?, ?, ?, ?)`,
      )
      .run(id, subject, at, at, subject, subject);
    if (inserted.changes === 1) insertMember(db, id, subject, "owner", subject);
  });
}

export function listWorkspaces(db: Database, subject: string): Workspace[] {
  const rows = db
    .query(
      `SELECT w.*, m.role AS member_role FROM workspaces w
       JOIN workspace_members m ON m.workspace_id = w.id
       WHERE m.subject = ?
       ORDER BY w.kind = 'team', w.name COLLATE NOCASE, w.id`,
    )
    .all(subject) as (WorkspaceRow & { member_role: Role })[];
  return rows.map((row) => toWorkspace(row, row.member_role));
}

export function createTeamWorkspace(db: Database, subject: string, name: string): Workspace {
  return transaction(db, () => {
    const id = randomUUID();
    const at = now();
    db.query(
      `INSERT INTO workspaces (id, name, kind, personal_subject, created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, 'team', NULL, ?, ?, ?, ?)`,
    ).run(id, name, at, at, subject, subject);
    insertMember(db, id, subject, "owner", subject);
    return toWorkspace(workspaceRow(db, id), "owner");
  });
}

export function renameWorkspace(
  db: Database,
  subject: string,
  id: string,
  name: string,
): Workspace {
  const role = workspaceAccess(db, id, subject);
  requireRole(role, "admin");
  db.query("UPDATE workspaces SET name = ?, updated_at = ?, updated_by = ? WHERE id = ?").run(
    name,
    now(),
    subject,
    id,
  );
  return toWorkspace(workspaceRow(db, id), role);
}

export function deleteWorkspace(db: Database, subject: string, id: string): void {
  const role = workspaceAccess(db, id, subject);
  requireRole(role, "owner");
  if (workspaceRow(db, id).kind === "personal")
    throw unprocessable("personal_workspace", "a personal workspace cannot be deleted");
  db.query("DELETE FROM workspaces WHERE id = ?").run(id);
}

export function listMembers(db: Database, subject: string, id: string): Member[] {
  workspaceAccess(db, id, subject);
  const rows = db
    .query("SELECT * FROM workspace_members WHERE workspace_id = ? ORDER BY subject")
    .all(id) as MemberRow[];
  return rows.map(toMember);
}

export function setMember(
  db: Database,
  subject: string,
  id: string,
  target: string,
  role: Role,
): Member {
  const callerRole = workspaceAccess(db, id, subject);
  requireRole(callerRole, "admin");
  if (workspaceRow(db, id).kind === "personal") throw unprocessable("personal_workspace", PERSONAL);
  return transaction(db, () => {
    const current = workspaceRole(db, id, target);
    if ((role === "owner" || current === "owner") && callerRole !== "owner") throw forbidden();
    if (current === "owner" && role !== "owner" && ownerCount(db, id) === 1) {
      throw unprocessable("last_owner", "a workspace must keep at least one owner");
    }
    insertMember(db, id, target, role, subject);
    const row = db
      .query("SELECT * FROM workspace_members WHERE workspace_id = ? AND subject = ?")
      .get(id, target) as MemberRow;
    return toMember(row);
  });
}

export function removeMember(db: Database, subject: string, id: string, target: string): void {
  const callerRole = workspaceAccess(db, id, subject);
  transaction(db, () => {
    const current = workspaceRole(db, id, target);
    if (!current) throw notFound("member");
    if (target !== subject) {
      requireRole(callerRole, "admin");
      if (current === "owner" && callerRole !== "owner") throw forbidden();
    }
    if (current === "owner" && ownerCount(db, id) === 1) {
      throw unprocessable("last_owner", "a workspace must keep at least one owner");
    }
    db.query("DELETE FROM workspace_members WHERE workspace_id = ? AND subject = ?").run(
      id,
      target,
    );
    // Access to restricted projects in this workspace goes with the membership.
    db.query(
      "DELETE FROM project_members WHERE subject = ? AND project_id IN (SELECT id FROM projects WHERE workspace_id = ?)",
    ).run(target, id);
    // So does their work: a task is never assigned to someone who cannot see it.
    db.query(
      `UPDATE tasks SET assignee_subject = NULL, version = version + 1, updated_at = ?, updated_by = ?
       WHERE assignee_subject = ? AND project_id IN (SELECT id FROM projects WHERE workspace_id = ?)`,
    ).run(now(), subject, target, id);
  });
}
