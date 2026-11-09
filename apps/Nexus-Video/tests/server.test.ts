import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-video", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    process.env.PORT = "43159";
    process.env.VIDEO_DB_PATH = ":memory:";
    process.env.NEXUS_VIDEO_ENABLE_CLOUD_INTEGRATION = "false";
    handle = await createServer(":memory:");
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
    expect(body.capabilities).toContain("hls-adaptive");
    expect(body.capabilities).toContain("webvtt-subtitles");
    expect(body.capabilities).toContain("video-chapters");
    expect(body.capabilities).toContain("playback-resume");
    expect(body.capabilities).toContain("chunked-upload");
  });

  it("POST /api/v1/video/channels creates channel", async () => {
    const res = await fetch(`${base}/api/v1/video/channels`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Tech Explored", description: "Deep tech analysis" }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.name).toBe("Tech Explored");
  });

  it("POST /api/v1/video/videos creates video with HLS and chapter metadata", async () => {
    const chRes = await fetch(`${base}/api/v1/video/channels`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Studio Broadcasts" }),
    });
    const ch = (await chRes.json()) as Record<string, unknown>;

    const res = await fetch(`${base}/api/v1/video/videos`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        title: "Autonomous Agent Architectures",
        description: "Comprehensive overview of agent frameworks",
        url: "https://example.com/stream/agents.mp4",
        channelId: ch.id,
        duration: 360,
      }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as any;
    expect(body.title).toBe("Autonomous Agent Architectures");
    expect(body.hlsUrl).toContain(`/api/v1/video/videos/${body.id}/hls/master.m3u8`);
    expect(body.chapters.length).toBeGreaterThan(0);
    expect(body.subtitles.length).toBeGreaterThan(0);
  });

  it("GET HLS master playlist and WebVTT subtitles return correct mime and content", async () => {
    const chRes = await fetch(`${base}/api/v1/video/channels`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "HLS Tests" }),
    });
    const ch = (await chRes.json()) as any;

    const createRes = await fetch(`${base}/api/v1/video/videos`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        title: "HLS Test Stream",
        url: "https://example.com/hls.mp4",
        channelId: ch.id,
        duration: 200,
      }),
    });
    const video = (await createRes.json()) as any;

    // HLS Master playlist
    const hlsRes = await fetch(`${base}/api/v1/video/videos/${video.id}/hls/master.m3u8`);
    expect(hlsRes.status).toBe(200);
    expect(hlsRes.headers.get("content-type")).toContain("mpegurl");
    const hlsText = await hlsRes.text();
    expect(hlsText).toContain("#EXTM3U");
    expect(hlsText).toContain("RESOLUTION=1920x1080");
    expect(hlsText).toContain("RESOLUTION=1280x720");

    // Subtitles WebVTT
    const vttRes = await fetch(`${base}/api/v1/video/videos/${video.id}/subtitles.vtt`);
    expect(vttRes.status).toBe(200);
    expect(vttRes.headers.get("content-type")).toContain("text/vtt");
    const vttText = await vttRes.text();
    expect(vttText).toContain("WEBVTT");

    // Chapters
    const chpRes = await fetch(`${base}/api/v1/video/videos/${video.id}/chapters`);
    expect(chpRes.status).toBe(200);
    const chapters = (await chpRes.json()) as any[];
    expect(chapters.length).toBeGreaterThan(0);
    expect(chapters[0].title).toBe("Introduction");
  });

  it("PUT and GET /api/v1/video/videos/:id/progress tracks playback position", async () => {
    const chRes = await fetch(`${base}/api/v1/video/channels`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Resume Tests" }),
    });
    const ch = (await chRes.json()) as any;

    const createRes = await fetch(`${base}/api/v1/video/videos`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        title: "Resume Video",
        url: "https://example.com/resume.mp4",
        channelId: ch.id,
        duration: 600,
      }),
    });
    const video = (await createRes.json()) as any;

    // Save progress at 145 seconds
    const putRes = await fetch(`${base}/api/v1/video/videos/${video.id}/progress`, {
      method: "PUT",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ userId: "test-user-1", position: 145.5, duration: 600 }),
    });
    expect(putRes.status).toBe(200);
    const putBody = (await putRes.json()) as any;
    expect(putBody.positionSeconds).toBe(145.5);
    expect(putBody.completed).toBe(false);

    // Retrieve progress
    const getRes = await fetch(`${base}/api/v1/video/videos/${video.id}/progress?userId=test-user-1`);
    expect(getRes.status).toBe(200);
    const getBody = (await getRes.json()) as any;
    expect(getBody.positionSeconds).toBe(145.5);
    expect(getBody.completed).toBe(false);
  });

  it("Chunked upload pipeline: init -> chunk -> complete creates video", async () => {
    const chRes = await fetch(`${base}/api/v1/video/channels`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Chunk Uploads" }),
    });
    const ch = (await chRes.json()) as any;

    // 1. Init upload
    const initRes = await fetch(`${base}/api/v1/video/upload/init`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ filename: "large-presentation.mp4", totalChunks: 3, fileSize: 15000000 }),
    });
    expect(initRes.status).toBe(201);
    const initBody = (await initRes.json()) as any;
    expect(initBody.uploadId).toBeDefined();

    // 2. Upload chunks
    for (let i = 0; i < 3; i++) {
      const chunkRes = await fetch(`${base}/api/v1/video/upload/chunk`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ uploadId: initBody.uploadId, chunkIndex: i }),
      });
      expect(chunkRes.status).toBe(200);
      const chunkBody = (await chunkRes.json()) as any;
      expect(chunkBody.uploadedChunks).toBe(i + 1);
    }

    // 3. Complete upload
    const compRes = await fetch(`${base}/api/v1/video/upload/complete`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        uploadId: initBody.uploadId,
        title: "Keynote 2026",
        channelId: ch.id,
        description: "Full event keynote",
      }),
    });
    expect(compRes.status).toBe(201);
    const compBody = (await compRes.json()) as any;
    expect(compBody.title).toBe("Keynote 2026");
    expect(compBody.hlsUrl).toBeDefined();
  });
});
