import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-portal", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    handle = await createServer();
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-portal");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-portal");
    expect(Array.isArray(body["capabilities"])).toBe(true);
    expect(body["phantom"]).toBeDefined();
    expect((body["phantom"] as any)["bound"]).toBe(true);
  });
});
