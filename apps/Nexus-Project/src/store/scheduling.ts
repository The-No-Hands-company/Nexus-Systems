import type { Database } from "bun:sqlite";
import { WorkingCalendar, fromDay, toDay } from "../schedule/calendar";
import { schedule } from "../schedule/engine";
import type { CalendarSpec, ScheduleInput, ScheduleResult, TaskState } from "../schedule/types";
import { now } from "./db";
import { taskKey } from "./queries";
import type { DependencyRow, ProjectRow, StatusCategory, TaskRow } from "./rows";

const STATE: Record<StatusCategory, TaskState> = {
  backlog: "open",
  unstarted: "open",
  started: "open",
  completed: "completed",
  canceled: "canceled",
};

export function weekdaysFromMask(mask: number): number[] {
  return [0, 1, 2, 3, 4, 5, 6].filter((day) => (mask >> day) & 1);
}

export function maskFromWeekdays(days: readonly number[]): number {
  return days.reduce((mask, day) => mask | (1 << day), 0);
}

export function loadCalendar(db: Database, project: ProjectRow): CalendarSpec {
  const rows = db.query("SELECT date, working FROM calendar_exceptions WHERE project_id = ? ORDER BY date").all(project.id) as {
    date: string;
    working: number;
  }[];
  return {
    workingWeekdays: weekdaysFromMask(project.working_weekdays),
    exceptions: rows.map((row) => ({ date: row.date, working: row.working === 1 })),
  };
}

export function projectCalendar(db: Database, project: ProjectRow): WorkingCalendar {
  return new WorkingCalendar(loadCalendar(db, project));
}

/** The last working day of a task starting on `start` and lasting `durationDays`. */
export function finishFor(calendar: WorkingCalendar, start: string, durationDays: number): string {
  const first = calendar.indexOf(toDay(start));
  return fromDay(calendar.dayAt(durationDays > 0 ? first + durationDays - 1 : first));
}

/** Working days from `start` through `finish`, both inclusive. Below 1 means finish precedes start. */
export function durationBetween(calendar: WorkingCalendar, start: string, finish: string): number {
  return calendar.indexOf(toDay(finish) + 1) - calendar.indexOf(toDay(start));
}

export function firstWorkingDay(calendar: WorkingCalendar, date: string): string {
  return fromDay(calendar.dayAt(calendar.indexOf(toDay(date))));
}

export function loadScheduleInput(db: Database, project: ProjectRow): ScheduleInput {
  const tasks = db
    .query("SELECT t.*, s.category FROM tasks t JOIN statuses s ON s.id = t.status_id WHERE t.project_id = ? ORDER BY t.number")
    .all(project.id) as (TaskRow & { category: StatusCategory })[];
  const links = db
    .query("SELECT * FROM dependencies WHERE project_id = ? ORDER BY created_at, id")
    .all(project.id) as DependencyRow[];
  return {
    projectStart: project.start_date,
    mode: project.schedule_mode,
    calendar: loadCalendar(db, project),
    tasks: tasks.map((row) => ({
      id: row.id,
      key: taskKey(project.key, row.number),
      number: row.number,
      parentId: row.parent_id,
      kind: row.kind,
      state: STATE[row.category],
      durationDays: row.duration_days,
      startDate: row.start_date,
      constraintType: row.constraint_type,
      constraintDate: row.constraint_date,
      deadline: row.deadline,
      progress: row.progress,
    })),
    links: links.map((row) => ({
      id: row.id,
      predecessorId: row.predecessor_id,
      successorId: row.successor_id,
      type: row.type,
      lagDays: row.lag_days,
    })),
  };
}

export function computeSchedule(db: Database, project: ProjectRow): ScheduleResult {
  return schedule(loadScheduleInput(db, project));
}

/**
 * Re-derives dates after a schedule-affecting write, inside the caller's
 * transaction. Auto mode: the engine owns leaf dates and writes them back.
 * Manual mode: nothing moves; violations are reported when the schedule is read.
 */
export function reschedule(db: Database, projectId: string, subject: string): void {
  const project = db.query("SELECT * FROM projects WHERE id = ?").get(projectId) as ProjectRow | null;
  if (!project || project.schedule_mode !== "auto") return;
  const result = computeSchedule(db, project);
  const current = new Map(
    (db.query("SELECT id, start_date, finish_date FROM tasks WHERE project_id = ?").all(projectId) as {
      id: string;
      start_date: string | null;
      finish_date: string | null;
    }[]).map((row) => [row.id, row]),
  );
  const update = db.query(
    "UPDATE tasks SET start_date = ?, finish_date = ?, version = version + 1, updated_at = ?, updated_by = ? WHERE id = ?",
  );
  const at = now();
  for (const task of result.tasks) {
    const row = current.get(task.id);
    if (!row || (row.start_date === task.earlyStart && row.finish_date === task.earlyFinish)) continue;
    update.run(task.earlyStart, task.earlyFinish, at, subject, task.id);
  }
}

/** After a calendar change a stored finish must follow the task's working-day duration. */
export function refreshDerivedFinishes(db: Database, project: ProjectRow, subject: string): void {
  const calendar = projectCalendar(db, project);
  const rows = db
    .query("SELECT id, start_date, finish_date, duration_days FROM tasks WHERE project_id = ? AND start_date IS NOT NULL AND duration_days IS NOT NULL")
    .all(project.id) as { id: string; start_date: string; finish_date: string | null; duration_days: number }[];
  const update = db.query(
    "UPDATE tasks SET start_date = ?, finish_date = ?, version = version + 1, updated_at = ?, updated_by = ? WHERE id = ?",
  );
  const at = now();
  for (const row of rows) {
    const start = firstWorkingDay(calendar, row.start_date);
    const finish = finishFor(calendar, start, row.duration_days);
    if (start !== row.start_date || finish !== row.finish_date) update.run(start, finish, at, subject, row.id);
  }
}
