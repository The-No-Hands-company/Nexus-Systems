import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { type ServerHandle, createMonitorServer } from "../src/server";

describe("nexus-monitor smoke contract", () => {
  let base = "";
  let handle: ServerHandle;

  beforeAll(() => {
    handle = createMonitorServer({ now: () => "2026-05-08T00:00:00.000Z" });
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
    expect(body["service"]).toBe("nexus-monitor");
    expect(body["status"]).toBe("ok");
  });

  it("GET /api/v1/monitor/status returns counters", async () => {
    const res = await fetch(`${base}/api/v1/monitor/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["status"]).toBe("ok");
    expect(typeof (body["monitor"] as Record<string, unknown>)["eventsReceived"]).toBe("number");
  });

  it("POST /api/v1/monitor/ingest accepts valid payload", async () => {
    const res = await fetch(`${base}/api/v1/monitor/ingest`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        kind: "event",
        source: "test-agent",
        timestamp: "2026-05-08T00:00:00.000Z",
      }),
    });
    expect(res.status).toBe(202);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["status"]).toBe("accepted");
    expect(typeof body["id"]).toBe("string");
  });

  it("POST /api/v1/monitor/ingest rejects invalid payload", async () => {
    const res = await fetch(`${base}/api/v1/monitor/ingest`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ kind: "invalid-kind", source: "test" }),
    });
    expect(res.status).toBe(400);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["code"]).toBe("VALIDATION_ERROR");
  });
});
