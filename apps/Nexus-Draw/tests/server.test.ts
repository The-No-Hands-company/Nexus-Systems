import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-draw", () => {
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
    expect(body["service"]).toBe("nexus-draw");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-draw");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });
});

describe("board routes", () => {
  it("PATCH updates board meta", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const created = await (await fetch(`${base}/api/v1/draw/boards`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Meta" }) })).json() as any;
    let res = await fetch(`${base}/api/v1/draw/boards/${created.id}`, { method: "PATCH", headers: { "content-type": "application/json" }, body: JSON.stringify({ gridSnap: true, background: "#000000" }) });
    expect(res.status).toBe(200);
    const got = await (await fetch(`${base}/api/v1/draw/boards/${created.id}`)).json() as any;
    expect(got.gridSnap).toBe(true);
    expect(got.background).toBe("#000000");
    close();
  });

  it("PATCH 404s for a missing board", async () => {
    const { server, close } = await createServer();
    const res = await fetch(`http://localhost:${server.port}/api/v1/draw/boards/nope`, { method: "PATCH", headers: { "content-type": "application/json" }, body: JSON.stringify({ gridSnap: true }) });
    expect(res.status).toBe(404);
    close();
  });

  it("DELETE removes a board and 404s the second time", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const created = await (await fetch(`${base}/api/v1/draw/boards`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Temp" }) })).json() as any;
    expect((await fetch(`${base}/api/v1/draw/boards/${created.id}`, { method: "DELETE" })).status).toBe(200);
    expect((await fetch(`${base}/api/v1/draw/boards/${created.id}`)).status).toBe(404);
    close();
  });
});
