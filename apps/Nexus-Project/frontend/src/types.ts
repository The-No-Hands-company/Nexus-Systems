export type Role = "viewer" | "member" | "admin" | "owner";
export type Priority = "none" | "low" | "medium" | "high" | "urgent";
export type TaskKind = "task" | "milestone";
export type ScheduleMode = "manual" | "auto";
export type StatusCategory = "backlog" | "unstarted" | "started" | "completed" | "canceled";
export type LinkType = "FS" | "SS" | "FF" | "SF";
export type ConstraintType = "asap" | "start_no_earlier_than";

export interface Workspace {
  id: string; name: string; kind: "personal" | "team";
  role: Role; memberCount: number;
  createdAt: string; updatedAt: string;
}
export interface Project {
  id: string; workspaceId: string; key: string; name: string;
  description: string | null; startDate: string | null;
  scheduleMode: ScheduleMode; visibility: "workspace" | "restricted";
  archived: boolean; taskCount: number;
  createdAt: string; updatedAt: string;
}
export interface Status {
  id: string; projectId: string; name: string;
  category: StatusCategory; position: number;
}
export interface Task {
  id: string; projectId: string; number: number; parentId: string | null;
  position: number; title: string; description: string | null;
  statusId: string; priority: Priority; assigneeSubject: string | null;
  kind: TaskKind; durationDays: number | null;
  startDate: string | null; finishDate: string | null;
  constraintType: ConstraintType; constraintDate: string | null;
  deadline: string | null; progress: number; version: number;
  createdAt: string; updatedAt: string;
}
export interface Dependency {
  id: string; projectId: string;
  predecessorId: string; successorId: string;
  type: LinkType; lagDays: number;
}
export interface TaskSchedule {
  id: string; earlyStart: string; earlyFinish: string;
  lateStart: string; lateFinish: string;
  totalFloat: number; freeFloat: number; critical: boolean;
}
export interface ScheduleResult {
  tasks: TaskSchedule[];
  summaries: Array<{ id: string; start: string; finish: string; progress: number }>;
  violations: Array<{ predecessorId: string; successorId: string; type: string; gapDays: number }>;
  warnings: string[];
  projectFinish: string | null;
  criticalPath: string[];
}
