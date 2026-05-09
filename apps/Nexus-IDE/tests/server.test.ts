import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { type ServerHandle, createIdeServer } from "../src/server";

describe("nexus-ide smoke contract", () => {
  let base = "";
  let handle: ServerHandle;

  beforeAll(() => {
    handle = createIdeServer({ now: () => "2026-05-08T00:00:00.000Z" });
    return new Promise<void>((resolve) => {
      handle.server.listen(0, "127.0.0.1", () => {
        const a = handle.server.address() as { port: number };
        base = `http://127.0.0.1:${a.port}`;
        resolve();
      });
    });
  });
  afterAll(() => handle.close());

  it("GET /health returns 200 with security headers", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    expect(res.headers.get("x-content-type-options")).toBe("nosniff");
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-ide");
  });

  it("GET /api/v1/ide/status returns counters", async () => {
    const res = await fetch(`${base}/api/v1/ide/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(typeof (body["ide"] as Record<string, unknown>)["sessionsCreated"]).toBe("number");
  });

  it("POST /api/v1/ide/session returns 201 with sessionId", async () => {
    const res = await fetch(`${base}/api/v1/ide/session`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        workspace: "/home/user/project",
        user: "alice",
        timestamp: "2026-05-08T00:00:00.000Z",
      }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["status"]).toBe("ok");
    expect(typeof body["sessionId"]).toBe("string");
  });

  it("POST /api/v1/ide/session rejects missing user", async () => {
    const res = await fetch(`${base}/api/v1/ide/session`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        workspace: "/home/user/project",
        timestamp: "2026-05-08T00:00:00.000Z",
      }),
    });
    expect(res.status).toBe(400);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["code"]).toBe("VALIDATION_ERROR");
  });
});
