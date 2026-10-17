import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";
describe("nexus-photos", () => {
  let base = "";
  let handle: ReturnType<typeof createServer>;
  beforeAll(async () => {
    process.env.PORT = "0";
    process.env.NEXUS_PHOTOS_ENABLE_CLOUD_INTEGRATION = "false";
    handle = createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => handle.close());
  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.service).toBe("nexus-photos");
    expect(body.status).toBe("ok");
  });
  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.service).toBe("nexus-photos");
    expect(Array.isArray(body.capabilities)).toBe(true);
  });
  it("POST /api/v1/photos/albums creates album", async () => {
    const res = await fetch(`${base}/api/v1/photos/albums`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Vacation", description: "Summer trip" }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.name).toBe("Vacation");
  });
  it("POST /api/v1/photos creates photo", async () => {
    const album = (await (
      await fetch(`${base}/api/v1/photos/albums`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ name: "Test" }),
      })
    ).json()) as Record<string, unknown>;
    if (typeof album.id !== "string") throw new Error("album id missing");
    const res = await fetch(`${base}/api/v1/photos`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        title: "Sunset",
        url: "https://example.com/sunset.jpg",
        albumId: album.id,
      }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.title).toBe("Sunset");
  });
  it("GET /api/v1/photos lists photos", async () => {
    const res = await fetch(`${base}/api/v1/photos`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>[];
    expect(Array.isArray(body)).toBe(true);
  });
  it("GET /api/v1/photos/albums lists albums", async () => {
    const res = await fetch(`${base}/api/v1/photos/albums`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>[];
    expect(Array.isArray(body)).toBe(true);
  });
});
