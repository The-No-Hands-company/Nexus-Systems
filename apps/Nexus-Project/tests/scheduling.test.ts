import type { Database } from "bun:sqlite";
import { beforeEach, describe, expect, it } from "bun:test";
import { randomUUID } from "node:crypto";
import { WorkingCalendar } from "../src/schedule/calendar";
import { openDatabase } from "../src/store/db";
import { createProject, updateProject } from "../src/store/projects";
import type { ProjectRow } from "../src/store/rows";
import {
  durationBetween,
  finishFor,
  firstWorkingDay,
  loadScheduleInput,
  maskFromWeekdays,
  reschedule,
  weekdaysFromMask,
} from "../src/store/scheduling";
import { updateStatus } from "../src/store/statuses";
import { createTeamWorkspace } from "../src/store/workspaces";

const OWNER = "usr-owner";
let db: Database;
let project: ProjectRow;

function statusId(category: string): string {
  return (
    db
      .query("SELECT id FROM statuses WHERE project_id = ? AND category = ?")
      .get(project.id, category) as { id: string }
  ).id;
}

function insertTask(
  number: number,
  fields: {
    duration?: number | null;
    start?: string | null;
    parent?: string | null;
    category?: string;
  },
): string {
  const id = randomUUID();
  db.query(
    `INSERT INTO tasks (id, project_id, number, parent_id, rank, title, status_id, duration_days, start_date,
       created_at, updated_at, created_by, updated_by)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'x', 'x', ?, ?)`,
  ).run(
    id,
    project.id,
    number,
    fields.parent ?? null,
    `r${number}`,
    `Task ${number}`,
    statusId(fields.category ?? "unstarted"),
    fields.duration === undefined ? 1 : fields.duration,
    fields.start ?? null,
    OWNER,
    OWNER,
  );
  return id;
}

function link(from: string, to: string): void {
  db.query(
    `INSERT INTO dependencies (id, project_id, predecessor_id, successor_id, type, lag_days, created_at, updated_at, created_by, updated_by)
     VALUES (?, ?, ?, ?, 'FS', 0, 'x', 'x', ?, ?)`,
  ).run(randomUUID(), project.id, from, to, OWNER, OWNER);
}

function dates(id: string) {
  return db.query("SELECT start_date, finish_date, version FROM tasks WHERE id = ?").get(id) as {
    start_date: string | null;
    finish_date: string | null;
    version: number;
  };
}

beforeEach(() => {
  db = openDatabase(":memory:");
  const ws = createTeamWorkspace(db, OWNER, "Team");
  const created = createProject(db, OWNER, ws.id, {
    key: "WEB",
    name: "Web",
    description: "",
    startDate: "2026-09-07",
    scheduleMode: "manual",
    visibility: "workspace",
  });
  project = db.query("SELECT * FROM projects WHERE id = ?").get(created.id) as ProjectRow;
});

describe("calendar helpers", () => {
  const cal = new WorkingCalendar({ workingWeekdays: [1, 2, 3, 4, 5], exceptions: [] });

  it("round-trips weekday masks", () => {
    expect(weekdaysFromMask(62)).toEqual([1, 2, 3, 4, 5]);
    expect(maskFromWeekdays([1, 2, 3, 4, 5])).toBe(62);
  });

  it("derives finish dates and inclusive durations in working days", () => {
    expect(finishFor(cal, "2026-09-07", 3)).toBe("2026-09-09");
    expect(finishFor(cal, "2026-09-11", 2)).toBe("2026-09-14");
    expect(finishFor(cal, "2026-09-07", 0)).toBe("2026-09-07");
    expect(durationBetween(cal, "2026-09-07", "2026-09-14")).toBe(6);
    expect(durationBetween(cal, "2026-09-09", "2026-09-07")).toBeLessThan(1);
    expect(firstWorkingDay(cal, "2026-09-05")).toBe("2026-09-07");
  });
});

describe("store to engine", () => {
  it("maps rows to engine input, with status categories as states", () => {
    const parent = insertTask(1, { duration: null });
    insertTask(2, { parent, category: "completed", start: "2026-09-07" });
    insertTask(3, { category: "canceled" });
    const input = loadScheduleInput(db, project);
    expect(input).toMatchObject({
      projectStart: "2026-09-07",
      mode: "manual",
      calendar: { workingWeekdays: [1, 2, 3, 4, 5], exceptions: [] },
    });
    expect(input.tasks.map((t) => [t.key, t.state, t.parentId === parent])).toEqual([
      ["WEB-1", "open", false],
      ["WEB-2", "completed", true],
      ["WEB-3", "canceled", false],
    ]);
  });

  it("leaves manual dates alone", () => {
    const a = insertTask(1, { duration: 3, start: "2026-09-14" });
    reschedule(db, project.id, OWNER);
    expect(dates(a)).toEqual({ start_date: "2026-09-14", finish_date: null, version: 1 });
  });

  it("writes auto dates, and only bumps versions that changed", () => {
    db.query("UPDATE projects SET schedule_mode = 'auto' WHERE id = ?").run(project.id);
    const a = insertTask(1, { duration: 3 });
    const b = insertTask(2, { duration: 2 });
    link(a, b);
    reschedule(db, project.id, OWNER);
    expect(dates(a)).toEqual({ start_date: "2026-09-07", finish_date: "2026-09-09", version: 2 });
    expect(dates(b)).toEqual({ start_date: "2026-09-10", finish_date: "2026-09-11", version: 2 });
    reschedule(db, project.id, OWNER);
    expect(dates(a).version).toBe(2);
    expect(dates(b).version).toBe(2);
  });

  it("reschedules when a project switches to auto or moves its start", () => {
    const a = insertTask(1, { duration: 2 });
    updateProject(db, OWNER, project.id, { scheduleMode: "auto" });
    expect(dates(a).start_date).toBe("2026-09-07");
    updateProject(db, OWNER, project.id, { startDate: "2026-09-14" });
    expect(dates(a)).toMatchObject({ start_date: "2026-09-14", finish_date: "2026-09-15" });
  });

  it("reschedules when a status becomes completed, pinning its tasks", () => {
    db.query("UPDATE projects SET schedule_mode = 'auto' WHERE id = ?").run(project.id);
    const a = insertTask(1, { duration: 3 });
    const b = insertTask(2, { duration: 1, category: "started" });
    link(a, b);
    reschedule(db, project.id, OWNER);
    expect(dates(b).start_date).toBe("2026-09-10");
    // A grows, which would push B, but B's status is about to become "completed".
    db.query("UPDATE tasks SET duration_days = 5 WHERE id = ?").run(a);
    updateStatus(db, OWNER, statusId("started"), { category: "completed" });
    // The category change rescheduled (A's finish moved) and pinned B where it was.
    expect(dates(a).finish_date).toBe("2026-09-11");
    expect(dates(b).start_date).toBe("2026-09-10");
  });
});
