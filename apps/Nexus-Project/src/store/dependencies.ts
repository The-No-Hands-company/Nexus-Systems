import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { type Role, projectAccess, requireRole } from "../access";
import { conflict, notFound, unprocessable } from "../http";
import { findCycle } from "../schedule/graph";
import type { LinkType } from "../schedule/types";
import { now, transaction } from "./db";
import { hasChildren, taskKey } from "./queries";
import type { DependencyRow, ProjectRow } from "./rows";
import { reschedule } from "./scheduling";

export const LINK_TYPES: readonly LinkType[] = ["FS", "SS", "FF", "SF"];

export interface Dependency {
  id: string;
  projectId: string;
  predecessorId: string;
  successorId: string;
  type: LinkType;
  lagDays: number;
}

export interface DependencyInput {
  predecessorId: string;
  successorId: string;
  type: LinkType;
  lagDays: number;
}

export interface DependencyPatch {
  type?: LinkType;
  lagDays?: number;
}

function toDependency(row: DependencyRow): Dependency {
  return {
    id: row.id,
    projectId: row.project_id,
    predecessorId: row.predecessor_id,
    successorId: row.successor_id,
    type: row.type,
    lagDays: row.lag_days,
  };
}

function linkAccess(
  db: Database,
  id: string,
  subject: string,
): { link: DependencyRow; project: ProjectRow; role: Role } {
  const link = db.query("SELECT * FROM dependencies WHERE id = ?").get(id) as DependencyRow | null;
  if (!link) throw notFound("dependency");
  try {
    return { link, ...projectAccess(db, link.project_id, subject) };
  } catch {
    throw notFound("dependency");
  }
}

/** A task a link may use. Elsewhere: 422 if the caller can see where it lives, 404 if not. */
function linkableTask(db: Database, project: ProjectRow, taskId: string, subject: string): void {
  const task = db.query("SELECT id, project_id FROM tasks WHERE id = ?").get(taskId) as {
    id: string;
    project_id: string;
  } | null;
  if (!task) throw notFound("task");
  if (task.project_id !== project.id) {
    try {
      projectAccess(db, task.project_id, subject);
    } catch {
      throw notFound("task");
    }
    throw unprocessable("cross_project_link", "dependencies must stay within one project");
  }
  if (hasChildren(db, task.id)) {
    throw unprocessable(
      "summary_link",
      "summary tasks are scheduled by their subtasks and cannot be linked",
    );
  }
}

export function listDependencies(db: Database, subject: string, projectId: string): Dependency[] {
  projectAccess(db, projectId, subject);
  const rows = db
    .query("SELECT * FROM dependencies WHERE project_id = ? ORDER BY created_at, id")
    .all(projectId) as DependencyRow[];
  return rows.map(toDependency);
}

export function createDependency(
  db: Database,
  subject: string,
  projectId: string,
  input: DependencyInput,
): Dependency {
  const { project, role } = projectAccess(db, projectId, subject);
  requireRole(role, "member");
  if (input.predecessorId === input.successorId)
    throw unprocessable("self_link", "a task cannot depend on itself");
  return transaction(db, () => {
    linkableTask(db, project, input.predecessorId, subject);
    linkableTask(db, project, input.successorId, subject);
    if (
      db
        .query("SELECT 1 FROM dependencies WHERE predecessor_id = ? AND successor_id = ?")
        .get(input.predecessorId, input.successorId)
    ) {
      throw conflict("duplicate_link", "these tasks are already linked");
    }
    const tasks = db
      .query("SELECT id, number FROM tasks WHERE project_id = ? ORDER BY number")
      .all(project.id) as {
      id: string;
      number: number;
    }[];
    const edges = (
      db
        .query(
          "SELECT predecessor_id, successor_id FROM dependencies WHERE project_id = ? ORDER BY created_at, id",
        )
        .all(project.id) as {
        predecessor_id: string;
        successor_id: string;
      }[]
    ).map((row) => ({ from: row.predecessor_id, to: row.successor_id }));
    edges.push({ from: input.predecessorId, to: input.successorId });
    const cycle = findCycle(
      tasks.map((task) => task.id),
      edges,
    );
    if (cycle) {
      const numbers = new Map(tasks.map((task) => [task.id, task.number]));
      const keys = cycle.map((id) => taskKey(project.key, numbers.get(id) as number));
      throw unprocessable(
        "dependency_cycle",
        `this link would create a cycle: ${keys.join(" → ")}`,
        { cycle: keys },
      );
    }
    const id = randomUUID();
    const at = now();
    db.query(
      `INSERT INTO dependencies (id, project_id, predecessor_id, successor_id, type, lag_days, created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
    ).run(
      id,
      project.id,
      input.predecessorId,
      input.successorId,
      input.type,
      input.lagDays,
      at,
      at,
      subject,
      subject,
    );
    reschedule(db, project.id, subject);
    return toDependency(
      db.query("SELECT * FROM dependencies WHERE id = ?").get(id) as DependencyRow,
    );
  });
}

export function updateDependency(
  db: Database,
  subject: string,
  id: string,
  patch: DependencyPatch,
): Dependency {
  const { link, role } = linkAccess(db, id, subject);
  requireRole(role, "member");
  return transaction(db, () => {
    db.query(
      "UPDATE dependencies SET type = ?, lag_days = ?, updated_at = ?, updated_by = ? WHERE id = ?",
    ).run(patch.type ?? link.type, patch.lagDays ?? link.lag_days, now(), subject, id);
    reschedule(db, link.project_id, subject);
    return toDependency(
      db.query("SELECT * FROM dependencies WHERE id = ?").get(id) as DependencyRow,
    );
  });
}

export function deleteDependency(db: Database, subject: string, id: string): void {
  const { link, role } = linkAccess(db, id, subject);
  requireRole(role, "member");
  transaction(db, () => {
    db.query("DELETE FROM dependencies WHERE id = ?").run(id);
    reschedule(db, link.project_id, subject);
  });
}
