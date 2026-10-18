import { afterAll, beforeAll, describe, expect, it, test } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-ai-hub", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;
  const originalCloudIntegration = process.env.NEXUS_AI_HUB_ENABLE_CLOUD_INTEGRATION;

  beforeAll(async () => {
    process.env.NEXUS_AI_HUB_ENABLE_CLOUD_INTEGRATION = "false";
    handle = await createServer({ cloudUrl: "http://127.0.0.1:1" });
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(async () => {
    await handle.close();
    if (originalCloudIntegration === undefined) delete process.env.NEXUS_AI_HUB_ENABLE_CLOUD_INTEGRATION;
    else process.env.NEXUS_AI_HUB_ENABLE_CLOUD_INTEGRATION = originalCloudIntegration;
  });

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-ai-hub");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-ai-hub");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("GET /api/v1/knowledge returns the contract index when Cloud is unavailable", async () => {
    const res = await fetch(`${base}/api/v1/knowledge`);
    expect(res.status).toBe(200);
    const body = await res.json() as { protocol: string; status: string; documents: Array<{ id: string; source: { ref: string } }> };
    expect(body.protocol).toBe("nexus-knowledge/1");
    expect(body.status).toBe("degraded");
    expect(body.documents.map((document) => document.id)).toContain("platform-auth");
    expect(body.documents.find((document) => document.id === "platform-auth")?.source.ref).toBe(
      "docs/architecture/nexus-platform-surfaces.md",
    );
  });

  it("GET /api/v1/knowledge/search filters the same provenance-bearing index", async () => {
    const res = await fetch(`${base}/api/v1/knowledge/search?q=authenticate+login`);
    expect(res.status).toBe(200);
    const body = await res.json() as { documents: Array<{ id: string }> };
    expect(body.documents.map((document) => document.id)).toEqual(["platform-auth"]);
  });
});
