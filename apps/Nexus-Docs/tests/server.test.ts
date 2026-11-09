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

    // 4b. Revisions & Restore
    const createRevRes = await fetch(`${base}/api/v1/documents/${doc.id}/revisions`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ title: "Platform Spec v1 Original", content: "Original version text" }),
    });
    expect(createRevRes.status).toBe(201);
    const createdRev = (await createRevRes.json()) as any;

    const listRevRes = await fetch(`${base}/api/v1/documents/${doc.id}/revisions`);
    expect(listRevRes.status).toBe(200);
    const revs = (await listRevRes.json()) as any[];
    expect(revs.length).toBeGreaterThanOrEqual(1);

    const restoreRes = await fetch(`${base}/api/v1/documents/${doc.id}/revisions/${createdRev.id}/restore`, {
      method: "POST",
    });
    expect(restoreRes.status).toBe(200);
    const restoredDoc = (await restoreRes.json()) as any;
    expect(restoredDoc.title).toBe("Platform Spec v1 Original");
    expect(restoredDoc.content).toBe("Original version text");

    // 5. Search Documents
    const searchRes = await fetch(`${base}/api/v1/documents/search?q=Original`);
    expect(searchRes.status).toBe(200);
    const searchDocs = (await searchRes.json()) as any[];
    expect(searchDocs.length).toBeGreaterThanOrEqual(1);

    // 6. Cover and Icon update
    const coverRes = await fetch(`${base}/api/v1/documents/${doc.id}`, {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        coverUrl: "linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%)",
        icon: "🚀",
      }),
    });
    expect(coverRes.status).toBe(200);
    const coverDoc = (await coverRes.json()) as any;
    expect(coverDoc.coverUrl).toBe("linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%)");
    expect(coverDoc.icon).toBe("🚀");

    // 7. Delete Document
    const delRes = await fetch(`${base}/api/v1/documents/${doc.id}`, {
      method: "DELETE",
    });
    expect(delRes.status).toBe(200);

    const checkRes = await fetch(`${base}/api/v1/documents/${doc.id}`);
    expect(checkRes.status).toBe(404);
  });

  it("WebSocket /api/v1/sync supports real-time collaboration and presence", async () => {
    const wsUrl = base.replace(/^http/, "ws") + "/api/v1/sync?docId=doc-collab-123&name=Alice";
    const wsUrl2 = base.replace(/^http/, "ws") + "/api/v1/sync?docId=doc-collab-123&name=Bob";

    const client1 = new WebSocket(wsUrl);
    const client2 = new WebSocket(wsUrl2);

    const received: any[] = [];

    await new Promise<void>((resolve, reject) => {
      let openCount = 0;
      const onOpen = () => {
        openCount++;
        if (openCount === 2) resolve();
      };
      client1.onopen = onOpen;
      client2.onopen = onOpen;
      client1.onerror = reject;
      client2.onerror = reject;
      setTimeout(() => reject(new Error("WS connection timed out")), 2000);
    });

    client1.onmessage = (event) => {
      try {
        received.push(JSON.parse(String(event.data)));
      } catch {
        received.push(event.data);
      }
    };

    // Client 2 broadcasts an edit event
    client2.send(
      JSON.stringify({
        type: "edit",
        docId: "doc-collab-123",
        delta: { title: "Collaborative Update" },
      })
    );

    // Wait briefly for client 1 to receive the message
    await new Promise((r) => setTimeout(r, 200));

    expect(received.some((m) => m.type === "edit" && m.delta?.title === "Collaborative Update")).toBe(true);

    client1.close();
    client2.close();
  });
});
