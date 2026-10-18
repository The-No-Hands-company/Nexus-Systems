import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-graphic", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    process.env.PORT = "43157";
    process.env.NEXUS_GRAPHIC_ENABLE_CLOUD_INTEGRATION = "false";
    handle = await createServer();
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.service).toBe("nexus-graphic");
    expect(body.status).toBe("ok");
    expect(body.phantom).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.service).toBe("nexus-graphic");
    expect(Array.isArray(body.capabilities)).toBe(true);
    expect(body.phantom).toBeDefined();
    const phantom = body.phantom;
    expect(
      phantom !== null && typeof phantom === "object" && "bound" in phantom && phantom.bound,
    ).toBe(true);
  });
});
