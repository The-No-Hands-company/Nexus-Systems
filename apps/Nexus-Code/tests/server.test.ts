import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-code", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    handle = await createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-code");
    expect(body["status"]).toBe("ok");
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-code");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });
});
