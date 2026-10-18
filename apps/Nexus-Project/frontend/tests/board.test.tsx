import { describe, it, expect, vi } from "vitest";
import { render, screen } from "@testing-library/react";
import { ApiContext } from "../src/runtime";
import type { ProjectApi } from "../src/api";
import { BoardView } from "../src/views/Board";
import type { Project, Status, Task } from "../src/types";

const mockProject: Project = {
  id: "p1", workspaceId: "w1", key: "TEST", name: "Test Project",
  description: null, startDate: null, scheduleMode: "manual",
  visibility: "workspace", archived: false, taskCount: 2,
  createdAt: "2026-01-01T00:00:00Z", updatedAt: "2026-01-01T00:00:00Z",
};

const mockStatuses: Status[] = [
  { id: "s1", projectId: "p1", name: "Todo", category: "unstarted", position: 0 },
  { id: "s2", projectId: "p1", name: "In Progress", category: "started", position: 1 },
  { id: "s3", projectId: "p1", name: "Done", category: "completed", position: 2 },
];

const mockTasks: Task[] = [
  {
    id: "t1", projectId: "p1", number: 1, parentId: null, position: 0,
    title: "First task", description: null, statusId: "s1", priority: "high",
    assigneeSubject: null, kind: "task", durationDays: null,
    startDate: null, finishDate: null, constraintType: "asap", constraintDate: null,
    deadline: null, progress: 0, version: 1,
    createdAt: "2026-01-01T00:00:00Z", updatedAt: "2026-01-01T00:00:00Z",
  },
  {
    id: "t2", projectId: "p1", number: 2, parentId: null, position: 1,
    title: "Second task", description: null, statusId: "s2", priority: "low",
    assigneeSubject: null, kind: "task", durationDays: null,
    startDate: null, finishDate: null, constraintType: "asap", constraintDate: null,
    deadline: null, progress: 50, version: 1,
    createdAt: "2026-01-01T00:00:00Z", updatedAt: "2026-01-01T00:00:00Z",
  },
];

function makeApi(overrides: Partial<ProjectApi> = {}) {
  return {
    listStatuses: vi.fn().mockResolvedValue(mockStatuses),
    listTasks: vi.fn().mockResolvedValue(mockTasks),
    updateTask: vi.fn(),
    moveTask: vi.fn(),
    ...overrides,
  } as unknown as Parameters<typeof ApiContext.Provider>[0]["value"];
}

describe("BoardView", () => {
  it("renders columns for each status", async () => {
    const api = makeApi();
    render(
      <ApiContext.Provider value={api}>
        <BoardView project={mockProject} />
      </ApiContext.Provider>
    );
    for (const s of mockStatuses) {
      expect(await screen.findByTestId(`column-${s.id}`)).toBeTruthy();
    }
  });

  it("renders task cards in correct columns", async () => {
    const api = makeApi();
    render(
      <ApiContext.Provider value={api}>
        <BoardView project={mockProject} />
      </ApiContext.Provider>
    );
    expect(await screen.findByText("First task")).toBeTruthy();
    expect(await screen.findByText("Second task")).toBeTruthy();
  });

  it("shows loading state initially", () => {
    const api = makeApi({
      listStatuses: vi.fn().mockReturnValue(new Promise(() => {})),
      listTasks: vi.fn().mockReturnValue(new Promise(() => {})),
    });
    render(
      <ApiContext.Provider value={api}>
        <BoardView project={mockProject} />
      </ApiContext.Provider>
    );
    expect(screen.getByText(/loading board/i)).toBeTruthy();
  });
});
