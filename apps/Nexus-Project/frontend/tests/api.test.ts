import { describe, it, expect, vi, beforeEach } from "vitest";
import { projectApi, ProjectApiError } from "../src/api";

const BASE = "";

function mockFetch(status: number, body: unknown) {
  return vi.fn().mockResolvedValue({
    ok: status >= 200 && status < 300,
    status,
    json: () => Promise.resolve(body),
  });
}

describe("projectApi", () => {
  beforeEach(() => { vi.restoreAllMocks(); });

  it("listWorkspaces calls correct endpoint", async () => {
    globalThis.fetch = mockFetch(200, { workspaces: [{ id: "ws1", name: "My WS" }] });
    const api = projectApi(BASE);
    const result = await api.listWorkspaces();
    expect(result).toHaveLength(1);
    expect(result[0].id).toBe("ws1");
    expect((globalThis.fetch as ReturnType<typeof vi.fn>).mock.calls[0][0]).toBe("/api/v1/project/workspaces");
  });

  it("listProjects includes workspace id in URL", async () => {
    globalThis.fetch = mockFetch(200, { projects: [] });
    const api = projectApi(BASE);
    await api.listProjects("ws123");
    const url = (globalThis.fetch as ReturnType<typeof vi.fn>).mock.calls[0][0] as string;
    expect(url).toContain("ws123");
  });

  it("updateTask sets If-Match header", async () => {
    globalThis.fetch = mockFetch(200, { id: "t1", version: 2 });
    const api = projectApi(BASE);
    await api.updateTask("t1", { title: "New" }, 3);
    const headers = (globalThis.fetch as ReturnType<typeof vi.fn>).mock.calls[0][1].headers as Headers;
    expect(headers.get("If-Match")).toBe("3");
  });

  it("throws ProjectApiError on non-ok response", async () => {
    globalThis.fetch = mockFetch(404, { error: "not_found" });
    const api = projectApi(BASE);
    await expect(api.getTask("missing")).rejects.toBeInstanceOf(ProjectApiError);
  });

  it("deleteTask returns undefined for 204", async () => {
    globalThis.fetch = vi.fn().mockResolvedValue({ ok: true, status: 204, json: () => Promise.resolve(undefined) });
    const api = projectApi(BASE);
    const result = await api.deleteTask("t1");
    expect(result).toBeUndefined();
  });

  it("listTasks appends query params for filters", async () => {
    globalThis.fetch = mockFetch(200, { tasks: [] });
    const api = projectApi(BASE);
    await api.listTasks("p1", { status: "done" });
    const url = (globalThis.fetch as ReturnType<typeof vi.fn>).mock.calls[0][0] as string;
    expect(url).toContain("status=done");
  });

  it("myTasks calls /me/tasks", async () => {
    globalThis.fetch = mockFetch(200, { tasks: [] });
    const api = projectApi(BASE);
    await api.myTasks();
    const url = (globalThis.fetch as ReturnType<typeof vi.fn>).mock.calls[0][0] as string;
    expect(url).toContain("/me/tasks");
  });
});
