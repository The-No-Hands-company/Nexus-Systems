import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { projectAccess, requireRole, taskAccess, workspaceRole } from "../access";
import { badRequest, conflict, unprocessable } from "../http";
import { rankBetween } from "../rank";
import type { WorkingCalendar } from "../schedule/calendar";
import { now, transaction } from "./db";
import { hasChildren, hasLinks, statusInProject, taskKey } from "./queries";
import type { ProjectRow, TaskRow } from "./rows";
import {
  durationBetween,
  finishFor,
  firstWorkingDay,
  projectCalendar,
  reschedule,
} from "./scheduling";
import { defaultStatusId } from "./statuses";

export const PRIORITIES = ["none", "low", "medium", "high", "urgent"] as const;
export type Priority = (typeof PRIORITIES)[number];

export interface Task {
  id: string;
  projectId: string;
  key: string;
  number: number;
  parentId: string | null;
  rank: string;
  title: string;
  description: string;
  statusId: string;
  priority: Priority;
  assigneeSubject: string | null;
  kind: "task" | "milestone";
  durationDays: number | null;
  startDate: string | null;
  finishDate: string | null;
  constraintType: "asap" | "start_no_earlier_than";
  constraintDate: string | null;
  deadline: string | null;
  progress: number;
  version: number;
  createdAt: string;
  updatedAt: string;
  createdBy: string;
  updatedBy: string;
}

export interface TaskFields {
  title?: string;
  description?: string;
  statusId?: string;
  priority?: Priority;
  assigneeSubject?: string | null;
  kind?: "task" | "milestone";
  durationDays?: number | null;
  startDate?: string | null;
  finishDate?: string | null;
  constraintType?: "asap" | "start_no_earlier_than";
  constraintDate?: string | null;
  deadline?: string | null;
  progress?: number;
}

export interface TaskCreate extends TaskFields {
  title: string;
  parentId?: string | null;
}

export interface TaskMove {
  parentId?: string | null;
  statusId?: string;
  afterId?: string | null;
}

export interface TaskFilters {
  statusId?: string;
  assignee?: string | null;
  parent?: string | null;
}

const SUMMARY_LOCKED = [
  "kind",
  "durationDays",
  "startDate",
  "finishDate",
  "constraintType",
  "constraintDate",
  "progress",
] as const;

type Schedule = Pick<
  TaskRow,
  | "kind"
  | "duration_days"
  | "start_date"
  | "finish_date"
  | "constraint_type"
  | "constraint_date"
  | "deadline"
  | "progress"
>;

const EMPTY_SCHEDULE: Schedule = {
  kind: "task",
  duration_days: null,
  start_date: null,
  finish_date: null,
  constraint_type: "asap",
  constraint_date: null,
  deadline: null,
  progress: 0,
};

export function toTask(row: TaskRow, projectKey: string): Task {
  return {
    id: row.id,
    projectId: row.project_id,
    key: taskKey(projectKey, row.number),
    number: row.number,
    parentId: row.parent_id,
    rank: row.rank,
    title: row.title,
    description: row.description,
    statusId: row.status_id,
    priority: row.priority,
    assigneeSubject: row.assignee_subject,
    kind: row.kind,
    durationDays: row.duration_days,
    startDate: row.start_date,
    finishDate: row.finish_date,
    constraintType: row.constraint_type,
    constraintDate: row.constraint_date,
    deadline: row.deadline,
    progress: row.progress,
    version: row.version,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
    createdBy: row.created_by,
    updatedBy: row.updated_by,
  };
}

function taskRow(db: Database, id: string): TaskRow {
  return db.query("SELECT * FROM tasks WHERE id = ?").get(id) as TaskRow;
}

/** Merges requested schedule fields into stored ones and derives the finish date. */
function resolveSchedule(
  calendar: WorkingCalendar,
  mode: "manual" | "auto",
  current: Schedule,
  patch: TaskFields,
): Schedule {
  const next: Schedule = { ...current };
  if (patch.kind !== undefined) next.kind = patch.kind;
  if (patch.durationDays !== undefined) next.duration_days = patch.durationDays;
  if (patch.constraintType !== undefined) next.constraint_type = patch.constraintType;
  if (patch.constraintDate !== undefined) next.constraint_date = patch.constraintDate;
  if (patch.deadline !== undefined) next.deadline = patch.deadline;
  if (patch.progress !== undefined) next.progress = patch.progress;
  if (patch.startDate !== undefined) {
    if (mode === "auto" && patch.startDate !== null) {
      // The engine owns start dates in auto mode; asking for one sets the constraint instead.
      next.constraint_type = "start_no_earlier_than";
      next.constraint_date = patch.startDate;
    } else {
      next.start_date = patch.startDate;
    }
  }

  if (next.constraint_type === "asap") next.constraint_date = null;
  else if (next.constraint_date === null)
    throw badRequest("constraintDate is required with start_no_earlier_than");

  if (next.kind === "milestone") {
    if (
      patch.durationDays !== undefined &&
      patch.durationDays !== null &&
      patch.durationDays !== 0
    ) {
      throw unprocessable("milestone_duration", "a milestone has no duration");
    }
    if (patch.finishDate !== undefined && patch.finishDate !== null) {
      throw unprocessable("milestone_duration", "a milestone has no finish date of its own");
    }
    next.duration_days = 0;
  } else {
    if (
      current.kind === "milestone" &&
      patch.durationDays === undefined &&
      patch.finishDate === undefined
    ) {
      next.duration_days = null;
    }
    if (next.duration_days === 0) {
      throw unprocessable(
        "zero_duration",
        'a task lasts at least one working day; use kind "milestone" for a zero-length event',
      );
    }
  }

  if (next.start_date !== null) next.start_date = firstWorkingDay(calendar, next.start_date);
  if (next.kind === "task" && patch.finishDate !== undefined) {
    if (patch.finishDate === null) {
      if (patch.durationDays === undefined) next.duration_days = null;
    } else {
      const anchor = patch.startDate ?? next.start_date;
      if (anchor === null) throw badRequest("finishDate needs a startDate");
      const duration = durationBetween(
        calendar,
        firstWorkingDay(calendar, anchor),
        patch.finishDate,
      );
      if (duration < 1)
        throw unprocessable("invalid_dates", "finishDate must be on or after startDate");
      next.duration_days = duration;
    }
  }
  next.finish_date =
    next.start_date !== null && next.duration_days !== null
      ? finishFor(calendar, next.start_date, next.duration_days)
      : null;
  return next;
}

function assertStatus(db: Database, projectId: string, statusId: string): void {
  if (!statusInProject(db, projectId, statusId))
    throw badRequest("statusId must be a status of this project");
}

function assertAssignee(db: Database, project: ProjectRow, subject: string): void {
  if (!workspaceRole(db, project.workspace_id, subject)) {
    throw unprocessable(
      "assignee_not_member",
      "tasks can only be assigned to members of the project's workspace",
    );
  }
}

function assertParent(
  db: Database,
  projectId: string,
  parentId: string,
  movingTaskId: string | null,
): void {
  const parent = db.query("SELECT id, project_id, kind FROM tasks WHERE id = ?").get(parentId) as {
    id: string;
    project_id: string;
    kind: string;
  } | null;
  if (!parent || parent.project_id !== projectId)
    throw unprocessable("invalid_parent", "parentId must be a task in this project");
  if (parent.kind === "milestone")
    throw unprocessable("milestone_children", "a milestone cannot have subtasks");
  if (hasLinks(db, parentId)) {
    throw unprocessable(
      "summary_link",
      "a task with dependencies cannot become a summary task; remove its links first",
    );
  }
  if (movingTaskId === null) return;
  for (let id: string | null = parentId; id !== null; ) {
    if (id === movingTaskId)
      throw unprocessable("wbs_cycle", "a task cannot move under itself or its own subtask");
    id = (
      db.query("SELECT parent_id FROM tasks WHERE id = ?").get(id) as { parent_id: string | null }
    ).parent_id;
  }
}

function assertVersion(task: TaskRow, expected: number, projectKey: string): void {
  if (task.version !== expected) {
    throw conflict("version_conflict", "this task was changed by someone else", {
      task: toTask(task, projectKey),
    });
  }
}

export function listTasks(
  db: Database,
  subject: string,
  projectId: string,
  filters: TaskFilters,
): Task[] {
  const { project } = projectAccess(db, projectId, subject);
  const clauses = ["project_id = ?"];
  const args: string[] = [projectId];
  if (filters.statusId !== undefined) {
    clauses.push("status_id = ?");
    args.push(filters.statusId);
  }
  if (filters.assignee !== undefined) {
    if (filters.assignee === null) clauses.push("assignee_subject IS NULL");
    else {
      clauses.push("assignee_subject = ?");
      args.push(filters.assignee);
    }
  }
  if (filters.parent !== undefined) {
    if (filters.parent === null) clauses.push("parent_id IS NULL");
    else {
      clauses.push("parent_id = ?");
      args.push(filters.parent);
    }
  }
  const rows = db
    .query(`SELECT * FROM tasks WHERE ${clauses.join(" AND ")} ORDER BY rank, id`)
    .all(...args) as TaskRow[];
  return rows.map((row) => toTask(row, project.key));
}

export function getTask(db: Database, subject: string, id: string): Task {
  const { task, project } = taskAccess(db, id, subject);
  return toTask(task, project.key);
}

export function createTask(
  db: Database,
  subject: string,
  projectId: string,
  input: TaskCreate,
): Task {
  const { project, role } = projectAccess(db, projectId, subject);
  requireRole(role, "member");
  return transaction(db, () => {
    const parentId = input.parentId ?? null;
    if (parentId !== null) assertParent(db, project.id, parentId, null);
    const statusId = input.statusId ?? defaultStatusId(db, project.id);
    assertStatus(db, project.id, statusId);
    if (input.assigneeSubject) assertAssignee(db, project, input.assigneeSubject);
    const fields = resolveSchedule(
      projectCalendar(db, project),
      project.schedule_mode,
      EMPTY_SCHEDULE,
      input,
    );

    const { next_number: number } = db
      .query("SELECT next_number FROM projects WHERE id = ?")
      .get(project.id) as { next_number: number };
    db.query("UPDATE projects SET next_number = next_number + 1 WHERE id = ?").run(project.id);
    const { last } = db
      .query("SELECT MAX(rank) AS last FROM tasks WHERE project_id = ?")
      .get(project.id) as { last: string | null };

    const id = randomUUID();
    const at = now();
    db.query(
      `INSERT INTO tasks (id, project_id, number, parent_id, rank, title, description, status_id, priority, assignee_subject,
         kind, duration_days, start_date, finish_date, constraint_type, constraint_date, deadline, progress, version,
         created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?, ?, ?)`,
    ).run(
      id,
      project.id,
      number,
      parentId,
      rankBetween(last, null),
      input.title,
      input.description ?? "",
      statusId,
      input.priority ?? "none",
      input.assigneeSubject ?? null,
      fields.kind,
      fields.duration_days,
      fields.start_date,
      fields.finish_date,
      fields.constraint_type,
      fields.constraint_date,
      fields.deadline,
      fields.progress,
      at,
      at,
      subject,
      subject,
    );
    reschedule(db, project.id, subject);
    return toTask(taskRow(db, id), project.key);
  });
}

export function updateTask(
  db: Database,
  subject: string,
  id: string,
  expected: number,
  patch: TaskFields,
): Task {
  const { project, role } = taskAccess(db, id, subject);
  requireRole(role, "member");
  return transaction(db, () => {
    const task = taskRow(db, id);
    assertVersion(task, expected, project.key);
    if (hasChildren(db, id) && SUMMARY_LOCKED.some((key) => patch[key] !== undefined)) {
      throw unprocessable("summary_fields", "a summary task's schedule comes from its subtasks");
    }
    if (patch.statusId !== undefined) assertStatus(db, project.id, patch.statusId);
    if (patch.assigneeSubject) assertAssignee(db, project, patch.assigneeSubject);
    const fields = resolveSchedule(
      projectCalendar(db, project),
      project.schedule_mode,
      task,
      patch,
    );
    db.query(
      `UPDATE tasks SET title = ?, description = ?, status_id = ?, priority = ?, assignee_subject = ?, kind = ?,
         duration_days = ?, start_date = ?, finish_date = ?, constraint_type = ?, constraint_date = ?, deadline = ?,
         progress = ?, version = version + 1, updated_at = ?, updated_by = ?
       WHERE id = ?`,
    ).run(
      patch.title ?? task.title,
      patch.description ?? task.description,
      patch.statusId ?? task.status_id,
      patch.priority ?? task.priority,
      patch.assigneeSubject === undefined ? task.assignee_subject : patch.assigneeSubject,
      fields.kind,
      fields.duration_days,
      fields.start_date,
      fields.finish_date,
      fields.constraint_type,
      fields.constraint_date,
      fields.deadline,
      fields.progress,
      now(),
      subject,
      id,
    );
    reschedule(db, project.id, subject);
    return toTask(taskRow(db, id), project.key);
  });
}

export function moveTask(
  db: Database,
  subject: string,
  id: string,
  expected: number,
  move: TaskMove,
): Task {
  const { project, role } = taskAccess(db, id, subject);
  requireRole(role, "member");
  return transaction(db, () => {
    const task = taskRow(db, id);
    assertVersion(task, expected, project.key);
    if (move.parentId !== undefined && move.parentId !== null)
      assertParent(db, project.id, move.parentId, id);
    if (move.statusId !== undefined) assertStatus(db, project.id, move.statusId);

    let rank = task.rank;
    if (move.afterId === null) {
      const { first } = db
        .query("SELECT MIN(rank) AS first FROM tasks WHERE project_id = ? AND id <> ?")
        .get(project.id, id) as {
        first: string | null;
      };
      rank = rankBetween(null, first);
    } else if (move.afterId !== undefined) {
      const after = db
        .query("SELECT rank FROM tasks WHERE id = ? AND project_id = ? AND id <> ?")
        .get(move.afterId, project.id, id) as {
        rank: string;
      } | null;
      if (!after)
        throw unprocessable("invalid_after", "afterId must be another task in this project");
      const { following } = db
        .query(
          "SELECT MIN(rank) AS following FROM tasks WHERE project_id = ? AND rank > ? AND id <> ?",
        )
        .get(project.id, after.rank, id) as { following: string | null };
      rank = rankBetween(after.rank, following);
    }

    db.query(
      "UPDATE tasks SET parent_id = ?, status_id = ?, rank = ?, version = version + 1, updated_at = ?, updated_by = ? WHERE id = ?",
    ).run(
      move.parentId === undefined ? task.parent_id : move.parentId,
      move.statusId ?? task.status_id,
      rank,
      now(),
      subject,
      id,
    );
    reschedule(db, project.id, subject);
    return toTask(taskRow(db, id), project.key);
  });
}

export function deleteTask(db: Database, subject: string, id: string): void {
  const { project, role } = taskAccess(db, id, subject);
  requireRole(role, "member");
  transaction(db, () => {
    // ON DELETE CASCADE removes the subtree and every link touching it.
    db.query("DELETE FROM tasks WHERE id = ?").run(id);
    reschedule(db, project.id, subject);
  });
}
