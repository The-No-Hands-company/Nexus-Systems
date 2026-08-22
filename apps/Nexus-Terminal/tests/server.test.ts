import { afterAll, beforeAll, describe, expect, it } from "bun:test"; import { createServer } from "../src/server";
describe("nexus-terminal", () => { let base = ""; let handle: Awaited<ReturnType<typeof createServer>> | undefined;
  const environmentKeys = ["NEXUS_BIND_HOST", "NEXUS_TERMINAL_ENABLED", "NEXUS_TERMINAL_ENABLE_CLOUD_INTEGRATION", "PORT"] as const;
  const originalEnvironment = new Map(environmentKeys.map((key) => [key, process.env[key]] as const));
  beforeAll(async () => {
    Reflect.deleteProperty(process.env, "NEXUS_BIND_HOST");
    Reflect.deleteProperty(process.env, "NEXUS_TERMINAL_ENABLED");
    process.env.NEXUS_TERMINAL_ENABLE_CLOUD_INTEGRATION = "false";
    process.env.PORT = "0";
    handle = await createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => {
    handle?.close();
    for (const [key, value] of originalEnvironment) {
      if (value === undefined) Reflect.deleteProperty(process.env, key);
      else process.env[key] = value;
    }
  });
  it("GET /health returns 200", async () => { const res = await fetch(`${base}/health`); expect(res.status).toBe(200); const body = await res.json() as Record<string, unknown>; expect(body["service"]).toBe("nexus-terminal"); expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined(); });
  it("GET /api/v1/status returns capabilities", async () => { const res = await fetch(`${base}/api/v1/status`); expect(res.status).toBe(200); const body = await res.json() as Record<string, unknown>; expect(body["service"]).toBe("nexus-terminal"); expect(Array.isArray(body["capabilities"])).toBe(true); });
  it("POST /api/v1/terminal/sessions creates session", async () => { const res = await fetch(`${base}/api/v1/terminal/sessions`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ userId: "dev1", type: "bash" }) }); expect(res.status).toBe(201); const body = await res.json() as any; expect(body.status).toBe("active"); });
  it("POST /api/v1/terminal/commands logs command", async () => { const session = await (await fetch(`${base}/api/v1/terminal/sessions`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ userId: "dev2", type: "sh" }) })).json() as any; const res = await fetch(`${base}/api/v1/terminal/commands`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ sessionId: session.id, command: "ls -la", output: "total 0", exitCode: 0 }) }); expect(res.status).toBe(201); const body = await res.json() as any; expect(body.command).toBe("ls -la"); });
  it("POST /api/v1/terminal/sessions/end ends session", async () => { const session = await (await fetch(`${base}/api/v1/terminal/sessions`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ userId: "dev3", type: "ssh" }) })).json() as any; const res = await fetch(`${base}/api/v1/terminal/sessions/end`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ id: session.id }) }); expect(res.status).toBe(200); });
  it("GET /api/v1/terminal/sessions lists sessions", async () => { const res = await fetch(`${base}/api/v1/terminal/sessions`); expect(res.status).toBe(200); const body = await res.json() as any[]; expect(Array.isArray(body)).toBe(true); });
});
