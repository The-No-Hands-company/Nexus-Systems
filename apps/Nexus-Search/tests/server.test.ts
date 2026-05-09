import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { type ServerHandle, createSearchServer } from "../src/server";

describe("nexus-search smoke contract", () => {
  let base = "";
  let handle: ServerHandle;

  beforeAll(() => {
    handle = createSearchServer({ now: () => "2026-05-08T00:00:00.000Z" });
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
    expect(body["service"]).toBe("nexus-search");
  });

  it("GET /api/v1/search/status returns counters", async () => {
    const res = await fetch(`${base}/api/v1/search/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(typeof (body["search"] as Record<string, unknown>)["queriesHandled"]).toBe("number");
  });

  it("POST /api/v1/search/query returns results", async () => {
    const res = await fetch(`${base}/api/v1/search/query`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ query: "nexus architecture", timestamp: "2026-05-08T00:00:00.000Z" }),
    });
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["status"]).toBe("ok");
    expect(Array.isArray(body["results"])).toBe(true);
  });

  it("POST /api/v1/search/query rejects missing query", async () => {
    const res = await fetch(`${base}/api/v1/search/query`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ timestamp: "2026-05-08T00:00:00.000Z" }),
    });
    expect(res.status).toBe(400);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["code"]).toBe("VALIDATION_ERROR");
  });
});
