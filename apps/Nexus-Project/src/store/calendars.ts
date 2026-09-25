import type { Database } from "bun:sqlite";
import { projectAccess, requireRole } from "../access";
import { badRequest } from "../http";
import { WorkingCalendar } from "../schedule/calendar";
import type { CalendarException } from "../schedule/types";
import { transaction } from "./db";
import type { ProjectRow } from "./rows";
import { loadCalendar, maskFromWeekdays, refreshDerivedFinishes, reschedule } from "./scheduling";

export interface ProjectCalendar {
  workingWeekdays: number[];
  exceptions: CalendarException[];
}

function snapshot(db: Database, project: ProjectRow): ProjectCalendar {
  const spec = loadCalendar(db, project);
  return { workingWeekdays: [...spec.workingWeekdays], exceptions: [...spec.exceptions] };
}

export function getCalendar(db: Database, subject: string, projectId: string): ProjectCalendar {
  return snapshot(db, projectAccess(db, projectId, subject).project);
}

export function setCalendar(
  db: Database,
  subject: string,
  projectId: string,
  input: ProjectCalendar,
): ProjectCalendar {
  const { project, role } = projectAccess(db, projectId, subject);
  requireRole(role, "admin");
  try {
    new WorkingCalendar(input);
  } catch (error) {
    throw badRequest((error as Error).message);
  }
  return transaction(db, () => {
    db.query("UPDATE projects SET working_weekdays = ? WHERE id = ?").run(
      maskFromWeekdays(input.workingWeekdays),
      project.id,
    );
    db.query("DELETE FROM calendar_exceptions WHERE project_id = ?").run(project.id);
    const insert = db.query(
      "INSERT INTO calendar_exceptions (project_id, date, working) VALUES (?, ?, ?)",
    );
    for (const exception of input.exceptions)
      insert.run(project.id, exception.date, exception.working ? 1 : 0);
    const updated = db.query("SELECT * FROM projects WHERE id = ?").get(project.id) as ProjectRow;
    refreshDerivedFinishes(db, updated, subject);
    reschedule(db, project.id, subject);
    return snapshot(db, updated);
  });
}
