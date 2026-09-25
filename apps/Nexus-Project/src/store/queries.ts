import type { Database } from "bun:sqlite";

export function hasChildren(db: Database, taskId: string): boolean {
  return Boolean(db.query("SELECT 1 FROM tasks WHERE parent_id = ? LIMIT 1").get(taskId));
}

export function hasLinks(db: Database, taskId: string): boolean {
  return Boolean(
    db
      .query("SELECT 1 FROM dependencies WHERE predecessor_id = ? OR successor_id = ? LIMIT 1")
      .get(taskId, taskId),
  );
}

export function statusInProject(db: Database, projectId: string, statusId: string): boolean {
  return Boolean(
    db.query("SELECT 1 FROM statuses WHERE id = ? AND project_id = ?").get(statusId, projectId),
  );
}

export function taskKey(projectKey: string, number: number): string {
  return `${projectKey}-${number}`;
}
