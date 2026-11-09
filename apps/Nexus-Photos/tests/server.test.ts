import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-photos", () => {
  let base = "";
  let handle: ReturnType<typeof createServer>;

  beforeAll(async () => {
    process.env.PORT = "43158";
    process.env.PHOTOS_DB_PATH = ":memory:";
    process.env.NEXUS_PHOTOS_ENABLE_CLOUD_INTEGRATION = "false";
    handle = createServer(":memory:");
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
    expect(body.capabilities).toContain("exif-parser");
    expect(body.capabilities).toContain("batch-actions");
    expect(body.capabilities).toContain("webp-thumbnails");
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

  it("POST /api/v1/photos creates photo with EXIF metadata and thumbnail", async () => {
    const albumRes = await fetch(`${base}/api/v1/photos/albums`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Nature Collection" }),
    });
    const album = (await albumRes.json()) as Record<string, unknown>;

    const res = await fetch(`${base}/api/v1/photos`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        title: "Sunset Over Alps",
        url: "https://example.com/alps-sunset.jpg",
        albumId: album.id,
        width: 4000,
        height: 3000,
        fileSize: 5242880,
      }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as any;
    expect(body.title).toBe("Sunset Over Alps");
    expect(body.exif).toBeDefined();
    expect(body.exif.model).toContain("ILCE-7M4");
    expect(body.thumbnailUrl).toContain(`/api/v1/photos/${body.id}/thumbnail`);
  });

  it("GET /api/v1/photos/:id/exif returns camera and lens metadata", async () => {
    const createRes = await fetch(`${base}/api/v1/photos`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        title: "Portrait Shot",
        url: "https://example.com/portrait.jpg",
      }),
    });
    const photo = (await createRes.json()) as any;

    const exifRes = await fetch(`${base}/api/v1/photos/${photo.id}/exif`);
    expect(exifRes.status).toBe(200);
    const exifBody = (await exifRes.json()) as any;
    expect(exifBody.photoId).toBe(photo.id);
    expect(exifBody.exif.iso).toBe(200);
    expect(exifBody.exif.fNumber).toBe(2.8);
    expect(exifBody.exif.gps.latitude).toBeCloseTo(55.6761, 2);
  });

  it("POST /api/v1/photos/:id/thumbnail triggers WebP thumbnail generation", async () => {
    const createRes = await fetch(`${base}/api/v1/photos`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        title: "City Lights",
        url: "https://example.com/city.jpg",
      }),
    });
    const photo = (await createRes.json()) as any;

    const thumbGenRes = await fetch(`${base}/api/v1/photos/${photo.id}/thumbnail`, {
      method: "POST",
    });
    expect(thumbGenRes.status).toBe(200);
    const genBody = (await thumbGenRes.json()) as any;
    expect(genBody.format).toBe("webp");

    const getThumbRes = await fetch(`${base}/api/v1/photos/${photo.id}/thumbnail.webp`);
    expect(getThumbRes.status).toBe(200);
    expect(getThumbRes.headers.get("content-type")).toBe("image/webp");
  });

  it("POST /api/v1/photos/batch/move and batch/download operate on multiple photos", async () => {
    const album1 = (await (
      await fetch(`${base}/api/v1/photos/albums`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ name: "Batch Source" }),
      })
    ).json()) as any;

    const album2 = (await (
      await fetch(`${base}/api/v1/photos/albums`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ name: "Batch Target" }),
      })
    ).json()) as any;

    const p1 = (await (
      await fetch(`${base}/api/v1/photos`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ title: "Batch 1", url: "https://example.com/b1.jpg", albumId: album1.id }),
      })
    ).json()) as any;

    const p2 = (await (
      await fetch(`${base}/api/v1/photos`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ title: "Batch 2", url: "https://example.com/b2.jpg", albumId: album1.id }),
      })
    ).json()) as any;

    // Batch download
    const dlRes = await fetch(`${base}/api/v1/photos/batch/download`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ ids: [p1.id, p2.id] }),
    });
    expect(dlRes.status).toBe(200);
    const dlBody = (await dlRes.json()) as any;
    expect(dlBody.count).toBe(2);
    expect(dlBody.zipName).toContain(".zip");

    // Batch move
    const moveRes = await fetch(`${base}/api/v1/photos/batch/move`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ ids: [p1.id, p2.id], albumId: album2.id }),
    });
    expect(moveRes.status).toBe(200);
    const moveBody = (await moveRes.json()) as any;
    expect(moveBody.updatedCount).toBe(2);

    // Verify album listing reflects counts
    const albumListRes = await fetch(`${base}/api/v1/photos/albums`);
    const albums = (await albumListRes.json()) as any[];
    const target = albums.find((a) => a.id === album2.id);
    expect(target.photo_count).toBe(2);

    // Batch delete
    const delRes = await fetch(`${base}/api/v1/photos/batch/delete`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ ids: [p1.id, p2.id] }),
    });
    expect(delRes.status).toBe(200);
    const delBody = (await delRes.json()) as any;
    expect(delBody.deletedCount).toBe(2);
  });
});
