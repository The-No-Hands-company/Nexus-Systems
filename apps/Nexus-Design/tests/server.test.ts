import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-design", () => {
  let base = "";
  let handle: ReturnType<typeof createServer>;

  beforeAll(async () => {
    process.env.PORT = "43156";
    process.env.NEXUS_DESIGN_ENABLE_CLOUD_INTEGRATION = "false";
    handle = createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.service).toBe("nexus-design");
    expect(body.status).toBe("ok");
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.service).toBe("nexus-design");
    expect(Array.isArray(body.capabilities)).toBe(true);
  });
});
