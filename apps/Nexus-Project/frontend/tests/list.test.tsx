import { describe, it, expect, vi } from "vitest";
import { render, screen } from "@testing-library/react";
import { ApiContext } from "../src/runtime";
import { ListView } from "../src/views/List";
import type { Project, Status, Task } from "../src/types";

const mockProject: Project = {
  id: "p1", workspaceId: "w1", key: "TEST", name: "Test",
  description: null, startDate: null, scheduleMode: "manual",
  visibility: "workspace", archived: false, taskCount: 3,
  createdAt: "2026-01-01T00:00:00Z", updatedAt: "2026-01-01T00:00:00Z",
};

const mockStatuses: Status[] = [
  { id: "s1", projectId: "p1", name: "Open", category: "unstarted", position: 0 },
];

function makeTask(id: string, n: number, title: string, parentId: string | null = null, pos = 0): Task {
  return {
    id, projectId: "p1", number: n, parentId, position: pos,
    title, description: null, statusId: "s1", priority: "none",
    assigneeSubject: null, kind: "task", durationDays: null,
    startDate: null, finishDate: null, constraintType: "asap", constraintDate: null,
    deadline: null, progress: 0, version: 1,
    createdAt: "2026-01-01T00:00:00Z", updatedAt: "2026-01-01T00:00:00Z",
  };
}

const mockTasks: Task[] = [
  makeTask("t1", 1, "Epic A", null, 0),
  makeTask("t2", 2, "Story 1", "t1", 0),
  makeTask("t3", 3, "Task X", "t2", 0),
];

function makeApi() {
  return {
    listStatuses: vi.fn().mockResolvedValue(mockStatuses),
    listTasks: vi.fn().mockResolvedValue(mockTasks),
    updateTask: vi.fn(),
  } as unknown as Parameters<typeof ApiContext.Provider>[0]["value"];
}

describe("ListView", () => {
  it("renders all tasks in a table", async () => {
    const api = makeApi();
    render(
      <ApiContext.Provider value={api}>
        <ListView project={mockProject} />
      </ApiContext.Provider>
    );
    expect(await screen.findByTestId("list-table")).toBeTruthy();
    expect(await screen.findByText("Epic A")).toBeTruthy();
    expect(await screen.findByText("Story 1")).toBeTruthy();
    expect(await screen.findByText("Task X")).toBeTruthy();
  });

  it("renders a row per task", async () => {
    const api = makeApi();
    render(
      <ApiContext.Provider value={api}>
        <ListView project={mockProject} />
      </ApiContext.Provider>
    );
    for (const t of mockTasks) {
      expect(await screen.findByTestId(`list-row-${t.id}`)).toBeTruthy();
    }
  });

  it("shows empty state when no tasks", async () => {
    const api = {
      listStatuses: vi.fn().mockResolvedValue(mockStatuses),
      listTasks: vi.fn().mockResolvedValue([]),
      updateTask: vi.fn(),
    } as unknown as Parameters<typeof ApiContext.Provider>[0]["value"];
    render(
      <ApiContext.Provider value={api}>
        <ListView project={mockProject} />
      </ApiContext.Provider>
    );
    expect(await screen.findByText(/no tasks yet/i)).toBeTruthy();
  });
});
