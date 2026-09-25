import type { Database } from "bun:sqlite";
import type { TaskRow } from "./rows";
import { type Task, toTask } from "./tasks";

export type AssignedTask = Task & { projectKey: string; projectName: string };

/** Open tasks assigned to `subject`, in every project they can currently see. */
export function listAssignedTasks(db: Database, subject: string): AssignedTask[] {
  const rows = db
    .query(
      `SELECT t.*, p.key AS project_key, p.name AS project_name
       FROM tasks t
       JOIN projects p ON p.id = t.project_id
       JOIN statuses s ON s.id = t.status_id
       JOIN workspace_members m ON m.workspace_id = p.workspace_id AND m.subject = ?
       WHERE t.assignee_subject = ?
         AND p.archived = 0
         AND s.category NOT IN ('completed', 'canceled')
         AND (p.visibility = 'workspace' OR m.role IN ('owner', 'admin')
              OR EXISTS (SELECT 1 FROM project_members pm WHERE pm.project_id = p.id AND pm.subject = ?))
       ORDER BY t.deadline IS NULL, t.deadline, p.key, t.number`,
    )
    .all(subject, subject, subject) as (TaskRow & { project_key: string; project_name: string })[];
  return rows.map((row) => ({
    ...toTask(row, row.project_key),
    projectKey: row.project_key,
    projectName: row.project_name,
  }));
}
