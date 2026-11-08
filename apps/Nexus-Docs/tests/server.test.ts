import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-docs", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    process.env.PORT = "0";
    handle = await createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-docs");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-docs");
    expect(Array.isArray(body["capabilities"])).toBe(true);
    expect(body["phantom"]).toBeDefined();
    expect((body["phantom"] as any)["bound"]).toBe(true);
  });

  it("Folder and Document full lifecycle works", async () => {
    // 1. Create Folder
    const folderRes = await fetch(`${base}/api/v1/folders`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name: "Architecture", color: "#10b981", icon: "📐" }),
    });
    expect(folderRes.status).toBe(201);
    const folder = (await folderRes.json()) as any;
    expect(folder.name).toBe("Architecture");

    // 2. Create Document
    const createRes = await fetch(`${base}/api/v1/documents`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        title: "Platform Spec",
        content: "# System Spec\n\nAll services operational.",
        folder: "Architecture",
        icon: "📘",
      }),
    });
    expect(createRes.status).toBe(201);
    const doc = (await createRes.json()) as any;
    expect(doc.title).toBe("Platform Spec");

    // 3. List Documents
    const listRes = await fetch(`${base}/api/v1/documents?folder=Architecture`);
    expect(listRes.status).toBe(200);
    const list = (await listRes.json()) as any[];
    expect(list.length).toBeGreaterThanOrEqual(1);

    // 4. Update Document
    const updateRes = await fetch(`${base}/api/v1/documents/${doc.id}`, {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ title: "Platform Spec v2", pinned: 1 }),
    });
    expect(updateRes.status).toBe(200);
    const updated = (await updateRes.json()) as any;
    expect(updated.title).toBe("Platform Spec v2");
    expect(updated.pinned).toBe(1);

    // 5. Search Documents
    const searchRes = await fetch(`${base}/api/v1/documents/search?q=operational`);
    expect(searchRes.status).toBe(200);
    const searchDocs = (await searchRes.json()) as any[];
    expect(searchDocs.length).toBeGreaterThanOrEqual(1);

    // 6. Delete Document
    const delRes = await fetch(`${base}/api/v1/documents/${doc.id}`, {
      method: "DELETE",
    });
    expect(delRes.status).toBe(200);

    const checkRes = await fetch(`${base}/api/v1/documents/${doc.id}`);
    expect(checkRes.status).toBe(404);
  });
});
