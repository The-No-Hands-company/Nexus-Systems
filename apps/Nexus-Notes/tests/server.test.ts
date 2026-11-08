import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-notes", () => {
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
    expect(body["service"]).toBe("nexus-notes");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-notes");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/notes/notebooks creates notebook", async () => {
    const res = await fetch(`${base}/api/v1/notes/notebooks`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "Work Notes", description: "Work related notes" }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as any;
    expect(body.name).toBe("Work Notes");
  });

  it("POST /api/v1/notes creates note", async () => {
    const res = await fetch(`${base}/api/v1/notes`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Test Note", content: "Hello World", notebook: "General" }),
    });
    expect(res.status).toBe(201);
    const body = (await res.json()) as any;
    expect(body.title).toBe("Test Note");
  });

  it("GET /api/v1/notes lists notes", async () => {
    const res = await fetch(`${base}/api/v1/notes`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as any[];
    expect(Array.isArray(body)).toBe(true);
  });

  it("GET /api/v1/notes/search searches notes", async () => {
    await (
      await fetch(`${base}/api/v1/notes`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ title: "Searchable", content: "find me" }),
      })
    ).json();
    const res = await fetch(`${base}/api/v1/notes/search?q=find`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as any[];
    expect(body.length).toBeGreaterThanOrEqual(1);
  });

  it("PATCH, PIN, and DELETE note operations work", async () => {
    // 1. Create a note
    const createRes = await fetch(`${base}/api/v1/notes`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Editable Note", content: "Initial content", notebook: "General" }),
    });
    expect(createRes.status).toBe(201);
    const note = (await createRes.json()) as any;

    // 2. Patch note
    const patchRes = await fetch(`${base}/api/v1/notes/${note.id}`, {
      method: "PATCH",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Updated Note", content: "Updated content", tags: "project,release" }),
    });
    expect(patchRes.status).toBe(200);
    const patched = (await patchRes.json()) as any;
    expect(patched.title).toBe("Updated Note");
    expect(patched.tags).toBe("project,release");

    // 3. Pin note
    const pinRes = await fetch(`${base}/api/v1/notes/${note.id}/pin`, {
      method: "POST",
    });
    expect(pinRes.status).toBe(200);
    const pinned = (await pinRes.json()) as any;
    expect(pinned.pinned).toBe(1);

    // 4. Delete note
    const delRes = await fetch(`${base}/api/v1/notes/${note.id}`, {
      method: "DELETE",
    });
    expect(delRes.status).toBe(200);

    // 5. Verify deleted
    const getRes = await fetch(`${base}/api/v1/notes/${note.id}`);
    expect(getRes.status).toBe(404);
  });

  it("GET /api/v1/notes/:id/backlinks returns linking notes", async () => {
    const target = (await (await fetch(`${base}/api/v1/notes`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Architecture Plan", content: "Core architecture details" }),
    })).json()) as any;

    await fetch(`${base}/api/v1/notes`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Project Overview", content: "See [[Architecture Plan]] for more" }),
    });

    const res = await fetch(`${base}/api/v1/notes/${target.id}/backlinks`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as any[];
    expect(body.some((n: any) => n.title === "Project Overview")).toBe(true);
  });
});
