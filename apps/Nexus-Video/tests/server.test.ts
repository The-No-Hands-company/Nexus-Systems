import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";
describe("nexus-video", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;
  beforeAll(async () => {
    process.env.PORT = "0";
    process.env.NEXUS_VIDEO_ENABLE_CLOUD_INTEGRATION = "false";
    handle = await createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => handle.close());
  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.service).toBe("nexus-video");
    expect(body.status).toBe("ok");
    expect(body.phantom).toBeDefined();
  });
  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.service).toBe("nexus-video");
    expect(Array.isArray(body.capabilities)).toBe(true);
  });
  it("POST /api/v1/video/channels creates channel", async () => {
    const res = await fetch(`${base}/api/v1/video/channels`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Test Channel", description: "A test channel" }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.name).toBe("Test Channel");
  });
  it("GET /api/v1/video/channels lists channels", async () => {
    const res = await fetch(`${base}/api/v1/video/channels`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>[];
    expect(Array.isArray(body)).toBe(true);
  });
  it("POST /api/v1/video/videos creates video", async () => {
    const chRes = await fetch(`${base}/api/v1/video/channels`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Chan" }),
    });
    const ch = (await chRes.json()) as Record<string, unknown>;
    if (typeof ch.id !== "string") throw new Error("channel id missing");
    const res = await fetch(`${base}/api/v1/video/videos`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        title: "Test Video",
        url: "https://example.com/video.mp4",
        channelId: ch.id,
        duration: 120,
      }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.title).toBe("Test Video");
  });
  it("POST /api/v1/video/playlists creates playlist", async () => {
    const res = await fetch(`${base}/api/v1/video/playlists`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "My Playlist" }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.name).toBe("My Playlist");
  });
});
