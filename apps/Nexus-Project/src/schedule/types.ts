/**
 * The scheduling engine's vocabulary. Pure data: no database, clock or I/O.
 * Dates are YYYY-MM-DD strings; durations and lags are whole working days.
 */

export type LinkType = "FS" | "SS" | "FF" | "SF";
export type TaskState = "open" | "completed" | "canceled";
export interface CalendarException { date: string; working: boolean }
export interface CalendarSpec { workingWeekdays: readonly number[]; exceptions: readonly CalendarException[] }
export interface ScheduleTask { id: string; key: string; number: number; parentId: string | null; kind: "task" | "milestone"; state: TaskState; durationDays: number | null; startDate: string | null; constraintType: "asap" | "start_no_earlier_than"; constraintDate: string | null; deadline: string | null; progress: number }
export interface ScheduleLink { id: string; predecessorId: string; successorId: string; type: LinkType; lagDays: number }
export interface ScheduleInput { projectStart: string; mode: "manual" | "auto"; calendar: CalendarSpec; tasks: readonly ScheduleTask[]; links: readonly ScheduleLink[] }
export interface TaskSchedule { id: string; earlyStart: string; earlyFinish: string; lateStart: string; lateFinish: string; totalFloat: number; freeFloat: number; critical: boolean }
export interface Violation { linkId: string; predecessorId: string; successorId: string; type: LinkType; gapDays: number }
export interface ScheduleWarning { code: "link_ignored"; linkId: string; taskId: string; reason: "unestimated" | "canceled" }
export interface SummaryRollup { id: string; start: string | null; finish: string | null; progress: number }
export interface ScheduleResult { projectFinish: string | null; tasks: TaskSchedule[]; summaries: SummaryRollup[]; criticalPath: string[]; violations: Violation[]; warnings: ScheduleWarning[] }
