import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { type ServerHandle, createAiHubServer } from "../src/server";

describe("nexus-ai-hub smoke contract", () => {
  let base = "";
  let handle: ServerHandle;

  beforeAll(() => {
    handle = createAiHubServer({ now: () => "2026-05-08T00:00:00.000Z" });
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
    expect(res.headers.get("x-request-id")).toBeTruthy();
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-ai-hub");
  });

  it("GET /api/v1/hub/providers/status returns providers", async () => {
    const res = await fetch(`${base}/api/v1/hub/providers/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(Array.isArray((body["hub"] as Record<string, unknown>)["providers"])).toBe(true);
  });

  it("POST /api/v1/hub/route accepts valid payload", async () => {
    const res = await fetch(`${base}/api/v1/hub/route`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        task: "summarize",
        tenant: "acme",
        priority: "normal",
        timestamp: "2026-05-08T00:00:00.000Z",
      }),
    });
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["status"]).toBe("ok");
    expect(typeof body["routeId"]).toBe("string");
  });

  it("POST /api/v1/hub/route rejects invalid priority", async () => {
    const res = await fetch(`${base}/api/v1/hub/route`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        task: "summarize",
        tenant: "acme",
        priority: "urgent",
        timestamp: "2026-05-08T00:00:00.000Z",
      }),
    });
    expect(res.status).toBe(400);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["code"]).toBe("VALIDATION_ERROR");
  });
});
