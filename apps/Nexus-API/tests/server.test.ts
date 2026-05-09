import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { type ServerHandle, createApiServer } from "../src/server";

describe("nexus-api smoke contract", () => {
  let base = "";
  let handle: ServerHandle;

  beforeAll(() => {
    handle = createApiServer();
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
    expect(body["service"]).toBe("nexus-api");
  });

  it("GET /api/v1/gateway/status returns counters", async () => {
    const res = await fetch(`${base}/api/v1/gateway/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(typeof body["routedTotal"]).toBe("number");
  });

  it("POST /api/v1/gateway/route routes valid request", async () => {
    const res = await fetch(`${base}/api/v1/gateway/route`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ target: "nexus-ai", method: "POST", path: "/api/v1/infer" }),
    });
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["ok"]).toBe(true);
    expect(body["routed"]).toBe(true);
  });

  it("POST /api/v1/gateway/route rejects missing target", async () => {
    const res = await fetch(`${base}/api/v1/gateway/route`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ method: "GET", path: "/api/v1/ping" }),
    });
    expect(res.status).toBe(400);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["code"]).toBe("VALIDATION_ERROR");
  });
});
