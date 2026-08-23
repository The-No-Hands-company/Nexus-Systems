import { describe, it, expect, beforeAll, afterAll } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-calendar", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    process.env.NEXUS_CALENDAR_DB = ":memory:";
    process.env.PORT = "0";
    handle = await createServer();
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const r = await fetch(`${base}/health`);
    expect(r.status).toBe(200);
  });

  it("creates and lists events", async () => {
    const create = await fetch(`${base}/api/v1/calendar/events`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Test Event", startTime: "2026-09-01T10:00", endTime: "2026-09-01T11:00" }),
    });
    expect(create.status).toBe(201);
    const ev = await create.json() as { id: string; title: string };
    expect(ev.title).toBe("Test Event");

    const list = await fetch(`${base}/api/v1/calendar/events?from=2026-09-01&to=2026-09-30`);
    const body = await list.json() as { events: { id: string }[] };
    expect(body.events.some((e) => e.id === ev.id)).toBe(true);
  });

  it("gets single event by id", async () => {
    const create = await fetch(`${base}/api/v1/calendar/events`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Get Me", startTime: "2026-09-02T10:00", endTime: "2026-09-02T11:00" }),
    });
    const ev = await create.json() as { id: string };
    const get = await fetch(`${base}/api/v1/calendar/events/${ev.id}`);
    expect(get.status).toBe(200);
    const body = await get.json() as { title: string };
    expect(body.title).toBe("Get Me");
  });

  it("patches event title", async () => {
    const create = await fetch(`${base}/api/v1/calendar/events`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Before Patch", startTime: "2026-09-03T10:00", endTime: "2026-09-03T11:00" }),
    });
    const ev = await create.json() as { id: string };
    const patch = await fetch(`${base}/api/v1/calendar/events/${ev.id}`, {
      method: "PATCH", headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "After Patch" }),
    });
    expect(patch.status).toBe(200);
    const body = await patch.json() as { title: string };
    expect(body.title).toBe("After Patch");
  });

  it("deletes event", async () => {
    const create = await fetch(`${base}/api/v1/calendar/events`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Delete Me", startTime: "2026-09-04T10:00", endTime: "2026-09-04T11:00" }),
    });
    const ev = await create.json() as { id: string };
    const del = await fetch(`${base}/api/v1/calendar/events/${ev.id}`, { method: "DELETE" });
    expect(del.status).toBe(200);
    const get = await fetch(`${base}/api/v1/calendar/events/${ev.id}`);
    expect(get.status).toBe(404);
  });
});
