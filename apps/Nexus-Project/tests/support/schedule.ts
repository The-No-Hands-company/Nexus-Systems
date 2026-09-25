import type {
  LinkType,
  ScheduleInput,
  ScheduleLink,
  ScheduleResult,
  ScheduleTask,
  TaskSchedule,
} from "../../src/schedule/types";

export const WEEKDAYS = { workingWeekdays: [1, 2, 3, 4, 5], exceptions: [] };
/** 2026-09-07 is a Monday. */
export const MONDAY = "2026-09-07";

export function task(id: string, number: number, fields: Partial<ScheduleTask> = {}): ScheduleTask {
  return {
    id,
    key: `T-${number}`,
    number,
    parentId: null,
    kind: "task",
    state: "open",
    durationDays: 1,
    startDate: null,
    constraintType: "asap",
    constraintDate: null,
    deadline: null,
    progress: 0,
    ...fields,
  };
}

export function link(
  id: string,
  predecessorId: string,
  successorId: string,
  type: LinkType = "FS",
  lagDays = 0,
): ScheduleLink {
  return { id, predecessorId, successorId, type, lagDays };
}

export function input(
  tasks: ScheduleTask[],
  links: ScheduleLink[] = [],
  overrides: Partial<ScheduleInput> = {},
): ScheduleInput {
  return { projectStart: MONDAY, mode: "auto", calendar: WEEKDAYS, tasks, links, ...overrides };
}

export function byId(result: ScheduleResult): Map<string, TaskSchedule> {
  return new Map(result.tasks.map((t) => [t.id, t]));
}

export function get(result: ScheduleResult, id: string): TaskSchedule {
  const found = byId(result).get(id);
  if (!found) throw new Error(`task ${id} was not scheduled`);
  return found;
}
