import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { type ServerHandle, createTestingServer } from "../src/server";

describe("nexus-testing smoke contract", () => {
  let base = "";
  let handle: ServerHandle;

  beforeAll(() => {
    handle = createTestingServer();
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
    expect(body["service"]).toBe("nexus-testing");
  });

  it("GET /api/v1/testing/status returns counters", async () => {
    const res = await fetch(`${base}/api/v1/testing/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(typeof body["submittedTotal"]).toBe("number");
  });

  it("POST /api/v1/testing/runs queues a run", async () => {
    const res = await fetch(`${base}/api/v1/testing/runs`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ suite: "integration", target: "nexus-monitor" }),
    });
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["ok"]).toBe(true);
    expect(body["status"]).toBe("queued");
    expect(typeof body["runId"]).toBe("string");
  });

  it("POST /api/v1/testing/runs rejects missing suite", async () => {
    const res = await fetch(`${base}/api/v1/testing/runs`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ target: "nexus-monitor" }),
    });
    expect(res.status).toBe(400);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["code"]).toBe("VALIDATION_ERROR");
  });
});
