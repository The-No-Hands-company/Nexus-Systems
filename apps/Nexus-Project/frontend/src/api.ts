import type { Dependency, Project, ScheduleResult, Status, Task, Workspace } from "./types";
import type { LinkType } from "./types";

export class ProjectApiError extends Error {
  constructor(readonly code: string, readonly status: number) { super(code); }
}

async function req<T>(base: string, path: string, init: RequestInit = {}): Promise<T> {
  const headers = new Headers(init.headers);
  headers.set("accept", "application/json");
  if (init.body) headers.set("content-type", "application/json");
  const res = await fetch(`${base}${path}`, { ...init, headers });
  if (!res.ok) {
    const body = await res.json().catch(() => ({})) as Record<string, unknown>;
    throw new ProjectApiError((body.error as string) ?? "unknown", res.status);
  }
  if (res.status === 204) return undefined as T;
  return res.json() as Promise<T>;
}

export function projectApi(base: string) {
  const r = <T>(path: string, init?: RequestInit) => req<T>(base, path, init);

  return {
    // Workspaces
    listWorkspaces: () => r<Workspace[]>("/api/v1/project/workspaces"),
    createWorkspace: (name: string, kind: "personal" | "team") =>
      r<Workspace>("/api/v1/project/workspaces", { method: "POST", body: JSON.stringify({ name, kind }) }),

    // Projects
    listProjects: (workspaceId: string) =>
      r<Project[]>(`/api/v1/project/workspaces/${workspaceId}/projects`),
    createProject: (workspaceId: string, body: { key: string; name: string; startDate?: string }) =>
      r<Project>(`/api/v1/project/workspaces/${workspaceId}/projects`, { method: "POST", body: JSON.stringify(body) }),
    getProject: (id: string) => r<Project>(`/api/v1/project/projects/${id}`),
    updateProject: (id: string, patch: Partial<Pick<Project, "name" | "description" | "startDate" | "scheduleMode">>) =>
      r<Project>(`/api/v1/project/projects/${id}`, { method: "PATCH", body: JSON.stringify(patch) }),

    // Statuses
    listStatuses: (projectId: string) =>
      r<Status[]>(`/api/v1/project/projects/${projectId}/statuses`),

    // Tasks
    listTasks: (projectId: string, filters?: { status?: string; assignee?: string; parent?: string }) => {
      const q = new URLSearchParams(filters as Record<string, string>).toString();
      return r<Task[]>(`/api/v1/project/projects/${projectId}/tasks${q ? `?${q}` : ""}`);
    },
    createTask: (projectId: string, body: Partial<Task> & { title: string }) =>
      r<Task>(`/api/v1/project/projects/${projectId}/tasks`, { method: "POST", body: JSON.stringify(body) }),
    getTask: (id: string) => r<Task>(`/api/v1/project/tasks/${id}`),
    updateTask: (id: string, patch: Partial<Task>, version: number) =>
      r<Task>(`/api/v1/project/tasks/${id}`, {
        method: "PATCH",
        headers: { "If-Match": String(version) },
        body: JSON.stringify(patch),
      }),
    moveTask: (id: string, body: { parentId?: string | null; position?: number; statusId?: string }) =>
      r<Task>(`/api/v1/project/tasks/${id}/move`, { method: "POST", body: JSON.stringify(body) }),
    deleteTask: (id: string) =>
      r<void>(`/api/v1/project/tasks/${id}`, { method: "DELETE" }),

    // Dependencies
    createDependency: (projectId: string, body: { predecessorId: string; successorId: string; type: LinkType; lagDays?: number }) =>
      r<Dependency>(`/api/v1/project/projects/${projectId}/dependencies`, { method: "POST", body: JSON.stringify(body) }),
    deleteDependency: (id: string) =>
      r<void>(`/api/v1/project/dependencies/${id}`, { method: "DELETE" }),

    // Schedule
    getSchedule: (projectId: string) =>
      r<ScheduleResult>(`/api/v1/project/projects/${projectId}/schedule`),

    // My tasks
    myTasks: () => r<Task[]>("/api/v1/project/me/tasks"),
  };
}

export type ProjectApi = ReturnType<typeof projectApi>;
