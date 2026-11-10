import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-music", () => {
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
    expect(body["service"]).toBe("nexus-music");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-music");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/music/tracks adds a track", async () => {
    const res = await fetch(`${base}/api/v1/music/tracks`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Bohemian Rhapsody", artist: "Queen", album: "A Night at the Opera", duration: 355, genre: "rock", url: "/music/bohemian.mp3" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.title).toBe("Bohemian Rhapsody");
    expect(body.artist).toBe("Queen");
  });

  it("GET /api/v1/music/tracks lists tracks", async () => {
    const res = await fetch(`${base}/api/v1/music/tracks`);
    expect(res.status).toBe(200);
    const body = await res.json() as any[];
    expect(Array.isArray(body)).toBe(true);
  });

  it("POST /api/v1/music/playlists creates a playlist", async () => {
    const res = await fetch(`${base}/api/v1/music/playlists`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Favorites", description: "My top tracks" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("Favorites");
  });

  it("POST /api/v1/music/playlists/add adds track to playlist", async () => {
    const trackRes = await fetch(`${base}/api/v1/music/tracks`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Song", artist: "Artist" }) });
    const track = await trackRes.json() as any;
    const playlistRes = await fetch(`${base}/api/v1/music/playlists`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "My Playlist" }) });
    const playlist = await playlistRes.json() as any;
    const res = await fetch(`${base}/api/v1/music/playlists/add`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ playlistId: playlist.id, trackId: track.id, order: 1 }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.playlistId).toBe(playlist.id);
    expect(body.trackId).toBe(track.id);
  });

  it("GET / serves Web Audio Player UI", async () => {
    const res = await fetch(`${base}/`);
    expect(res.status).toBe(200);
    expect(res.headers.get("content-type")).toContain("text/html");
    const html = await res.text();
    expect(html).toContain("Nexus Music");
  });

  it("GET /api/v1/music/tracks/:id/stream supports audio range streaming", async () => {
    const res = await fetch(`${base}/api/v1/music/tracks/sample-track/stream`, {
      headers: { range: "bytes=0-1023" },
    });
    expect(res.status).toBe(206);
    expect(res.headers.get("content-type")).toBe("audio/mpeg");
    expect(res.headers.get("accept-ranges")).toBe("bytes");
    expect(res.headers.get("content-range")).toContain("bytes 0-1023/");
  });
});
