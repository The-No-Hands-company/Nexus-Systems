import { afterAll, beforeAll, describe, expect, it } from "bun:test"; import { createServer } from "../src/server";
describe("nexus-tasks", () => { let base = ""; let handle: Awaited<ReturnType<typeof createServer>>;
  beforeAll(async () => { handle = await createServer(); await new Promise((r) => setTimeout(r, 200)); base = `http://127.0.0.1:${handle.server.port}`; });
  afterAll(() => handle.close());
  it("GET /health returns 200", async () => { const res = await fetch(`${base}/health`); expect(res.status).toBe(200); const body = await res.json() as Record<string, unknown>; expect(body["service"]).toBe("nexus-tasks"); expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined(); });
  it("GET /api/v1/status returns capabilities", async () => { const res = await fetch(`${base}/api/v1/status`); expect(res.status).toBe(200); const body = await res.json() as Record<string, unknown>; expect(body["service"]).toBe("nexus-tasks"); expect(Array.isArray(body["capabilities"])).toBe(true); });
  it("POST /api/v1/tasks/lists creates list", async () => { const res = await fetch(`${base}/api/v1/tasks/lists`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Work", description: "Work tasks" }) }); expect(res.status).toBe(201); const body = await res.json() as any; expect(body.name).toBe("Work"); });
  it("POST /api/v1/tasks creates task", async () => { const list = await (await fetch(`${base}/api/v1/tasks/lists`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Personal" }) })).json() as any; const res = await fetch(`${base}/api/v1/tasks`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Buy groceries", description: "Milk and bread", listId: list.id, priority: "high" }) }); expect(res.status).toBe(201); const body = await res.json() as any; expect(body.title).toBe("Buy groceries"); });
  it("POST /api/v1/tasks/complete completes task", async () => { const list = await (await fetch(`${base}/api/v1/tasks/lists`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Done" }) })).json() as any; const task = await (await fetch(`${base}/api/v1/tasks`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Task to complete", listId: list.id }) })).json() as any; const res = await fetch(`${base}/api/v1/tasks/complete`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ id: task.id }) }); expect(res.status).toBe(200); });
  it("GET /api/v1/tasks lists tasks", async () => { const res = await fetch(`${base}/api/v1/tasks`); expect(res.status).toBe(200); const body = await res.json() as any[]; expect(Array.isArray(body)).toBe(true); });
  it("GET / serves Interactive Checklist UI", async () => {
    const res = await fetch(`${base}/`);
    expect(res.status).toBe(200);
    expect(res.headers.get("content-type")).toContain("text/html");
    const html = await res.text();
    expect(html).toContain("Nexus Tasks");
  });
  it("GET /api/v1/tasks/assignees returns team members", async () => {
    const res = await fetch(`${base}/api/v1/tasks/assignees`);
    expect(res.status).toBe(200);
    const body = await res.json() as any[];
    expect(Array.isArray(body)).toBe(true);
    expect(body.length).toBeGreaterThan(0);
  });
});
