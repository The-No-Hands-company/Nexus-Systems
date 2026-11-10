import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createFilesServer } from "../src/server";

describe("nexus-files", () => {
  let base = "";
  let handle: ReturnType<typeof createFilesServer>;

  beforeAll(async () => {
    handle = createFilesServer();
    await new Promise((r) => setTimeout(r, 200));
    const port = handle.server.port;
    base = `http://127.0.0.1:${port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-files");
  });

  it("GET /api/v1/files returns file list", async () => {
    const res = await fetch(`${base}/api/v1/files`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(Array.isArray(body["files"])).toBe(true);
  });

  it("POST /api/v1/files accepts metadata payload", async () => {
    const res = await fetch(`${base}/api/v1/files`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        name: "report.pdf",
        contentType: "application/pdf",
        size: 4096,
        timestamp: "2026-05-08T00:00:00.000Z",
      }),
    });
    expect(res.status).toBe(202);
    const body = await res.json() as Record<string, unknown>;
    expect(body["status"]).toBe("accepted");
  });

  it("GET /api/v1/files/:id returns metadata", async () => {
    const create = await fetch(`${base}/api/v1/files`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        name: "test.txt",
        contentType: "text/plain",
        size: 100,
        timestamp: new Date().toISOString(),
      }),
    });
    const created = await create.json() as { id: string };

    const res = await fetch(`${base}/api/v1/files/${created.id}`);
    expect(res.status).toBe(200);
    const body = await res.json() as { file: { name: string } };
    expect(body.file.name).toBe("test.txt");
  });

  it("DELETE /api/v1/files/:id soft deletes", async () => {
    const create = await fetch(`${base}/api/v1/files`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        name: "to-delete.txt",
        contentType: "text/plain",
        size: 0,
        timestamp: new Date().toISOString(),
      }),
    });
    const created = await create.json() as { id: string };

    const del = await fetch(`${base}/api/v1/files/${created.id}`, { method: "DELETE" });
    expect(del.status).toBe(200);

    const get = await fetch(`${base}/api/v1/files/${created.id}`);
    expect(get.status).toBe(404);
  });

  it("GET / serves Web File Explorer UI", async () => {
    const res = await fetch(`${base}/`);
    expect(res.status).toBe(200);
    expect(res.headers.get("content-type")).toContain("text/html");
    const html = await res.text();
    expect(html).toContain("Nexus Files");
  });

  it("POST /api/v1/files/:id/share generates expiring share link", async () => {
    const create = await fetch(`${base}/api/v1/files`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        name: "shared-doc.txt",
        contentType: "text/plain",
        size: 50,
      }),
    });
    const created = await create.json() as { id: string };

    const share = await fetch(`${base}/api/v1/files/${created.id}/share`, { method: "POST" });
    expect(share.status).toBe(200);
    const body = await share.json() as { shareUrl: string; token: string };
    expect(body.shareUrl).toContain("/share/");
  });
});
