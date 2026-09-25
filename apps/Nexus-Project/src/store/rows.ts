/** Row shapes exactly as stored. Mapped to camelCase API objects by each store module. */
export type Role = "viewer" | "member" | "admin" | "owner";
export type StatusCategory = "backlog" | "unstarted" | "started" | "completed" | "canceled";

interface Audit {
  created_at: string;
  updated_at: string;
  created_by: string;
  updated_by: string;
}

export interface WorkspaceRow extends Audit {
  id: string;
  name: string;
  kind: "personal" | "team";
  personal_subject: string | null;
}

export interface MemberRow extends Audit {
  workspace_id: string;
  subject: string;
  role: Role;
}

export interface ProjectRow extends Audit {
  id: string;
  workspace_id: string;
  key: string;
  name: string;
  description: string;
  start_date: string;
  schedule_mode: "manual" | "auto";
  visibility: "workspace" | "restricted";
  archived: number;
  next_number: number;
  working_weekdays: number;
}

export interface StatusRow extends Audit {
  id: string;
  project_id: string;
  name: string;
  category: StatusCategory;
  position: number;
}

export interface TaskRow extends Audit {
  id: string;
  project_id: string;
  number: number;
  parent_id: string | null;
  rank: string;
  title: string;
  description: string;
  status_id: string;
  priority: "none" | "low" | "medium" | "high" | "urgent";
  assignee_subject: string | null;
  kind: "task" | "milestone";
  duration_days: number | null;
  start_date: string | null;
  finish_date: string | null;
  constraint_type: "asap" | "start_no_earlier_than";
  constraint_date: string | null;
  deadline: string | null;
  progress: number;
  version: number;
}

export interface DependencyRow extends Audit {
  id: string;
  project_id: string;
  predecessor_id: string;
  successor_id: string;
  type: "FS" | "SS" | "FF" | "SF";
  lag_days: number;
}
