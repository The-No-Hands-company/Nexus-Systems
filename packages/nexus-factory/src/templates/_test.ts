import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createNexusServer } from "@nexus/core";
import type { NexusRequestContext, NexusServerHandle } from "@nexus/core";

// Import your router — replace this with the actual import from your server module
async function router(ctx: NexusRequestContext): Promise<void> {
  // Your routes here (same as in src/index.ts, or import from a shared module)
  ctx.error(404, "not found", "NOT_FOUND");
}

describe("{{APP_NAME}} smoke contract", () => {
  let base = "";
  let handle: NexusServerHandle;

  beforeAll(() => {
    handle = createNexusServer(router, {
      serviceName: "{{APP_NAME}}",
      now: () => "2026-06-10T00:00:00.000Z",
    });
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
    expect(body["service"]).toBe("{{APP_NAME}}");
    expect(body["status"]).toBe("ok");
  });

  // TODO: add your own tests here
});
