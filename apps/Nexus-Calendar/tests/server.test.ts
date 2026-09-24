import { afterAll, beforeAll, describe, expect, it, mock, spyOn } from "bun:test";

const shutdownOrder: string[] = [];
const stopHeartbeat = mock(() => {
  shutdownOrder.push("heartbeat");
});
const startHeartbeat = mock(() => stopHeartbeat);
mock.module("../src/cloud", () => ({ startHeartbeat }));
const { createServer } = await import("../src/server");

describe("nexus-calendar", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    process.env.NEXUS_CALENDAR_DB = ":memory:";
    process.env.PORT = "0";
    process.env.NEXUS_CALENDAR_DASHBOARD_SECRET = "calendar-server-test-secret"; // pragma: allowlist secret
    handle = await createServer();
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => handle.close());

  function dashboardHeaders(subject: string, extra: ConstructorParameters<typeof Headers>[0] = {}): Headers {
    const headers = new Headers(extra);
    headers.set("x-nexus-subject", subject);
    headers.set("x-nexus-dashboard-secret", "calendar-server-test-secret");
    return headers;
  }

  async function createOwnedEvent(subject: string, title: string): Promise<{ id: string }> {
    const response = await fetch(`${base}/api/v1/calendar/events`, {
      method: "POST",
      headers: dashboardHeaders(subject, { "content-type": "application/json" }),
      body: JSON.stringify({ title, startTime: "2026-09-01T10:00:00.000Z", endTime: "2026-09-01T11:00:00.000Z" }),
    });
    expect(response.status).toBe(201);
    return response.json() as Promise<{ id: string }>;
  }

  it("GET /health returns 200", async () => {
    const r = await fetch(`${base}/health`);
    expect(r.status).toBe(200);
  });

  it("creates and lists events", async () => {
    const create = await fetch(`${base}/api/v1/calendar/events`, {
      method: "POST", headers: dashboardHeaders("usr-alice", { "content-type": "application/json" }),
      body: JSON.stringify({ title: "Test Event", startTime: "2026-09-01T10:00", endTime: "2026-09-01T11:00" }),
    });
    expect(create.status).toBe(201);
    const ev = await create.json() as { id: string; title: string };
    expect(ev.title).toBe("Test Event");

    const list = await fetch(`${base}/api/v1/calendar/events?from=2026-09-01&to=2026-09-30`, {
      headers: dashboardHeaders("usr-alice"),
    });
    const body = await list.json() as { events: { id: string }[] };
    expect(body.events.some((e) => e.id === ev.id)).toBe(true);
  });

  it("gets single event by id", async () => {
    const create = await fetch(`${base}/api/v1/calendar/events`, {
      method: "POST", headers: dashboardHeaders("usr-alice", { "content-type": "application/json" }),
      body: JSON.stringify({ title: "Get Me", startTime: "2026-09-02T10:00", endTime: "2026-09-02T11:00" }),
    });
    const ev = await create.json() as { id: string };
    const get = await fetch(`${base}/api/v1/calendar/events/${ev.id}`, { headers: dashboardHeaders("usr-alice") });
    expect(get.status).toBe(200);
    const body = await get.json() as { title: string };
    expect(body.title).toBe("Get Me");
  });

  it("patches event title", async () => {
    const create = await fetch(`${base}/api/v1/calendar/events`, {
      method: "POST", headers: dashboardHeaders("usr-alice", { "content-type": "application/json" }),
      body: JSON.stringify({ title: "Before Patch", startTime: "2026-09-03T10:00", endTime: "2026-09-03T11:00" }),
    });
    const ev = await create.json() as { id: string };
    const patch = await fetch(`${base}/api/v1/calendar/events/${ev.id}`, {
      method: "PATCH", headers: dashboardHeaders("usr-alice", { "content-type": "application/json" }),
      body: JSON.stringify({ title: "After Patch" }),
    });
    expect(patch.status).toBe(200);
    const body = await patch.json() as { title: string };
    expect(body.title).toBe("After Patch");
  });

  it("deletes event", async () => {
    const create = await fetch(`${base}/api/v1/calendar/events`, {
      method: "POST", headers: dashboardHeaders("usr-alice", { "content-type": "application/json" }),
      body: JSON.stringify({ title: "Delete Me", startTime: "2026-09-04T10:00", endTime: "2026-09-04T11:00" }),
    });
    const ev = await create.json() as { id: string };
    const del = await fetch(`${base}/api/v1/calendar/events/${ev.id}`, { method: "DELETE", headers: dashboardHeaders("usr-alice") });
    expect(del.status).toBe(200);
    const get = await fetch(`${base}/api/v1/calendar/events/${ev.id}`, { headers: dashboardHeaders("usr-alice") });
    expect(get.status).toBe(404);
  });

  it("returns 401 when a request has no trusted identity", async () => {
    const response = await fetch(`${base}/api/v1/calendar/events`);
    expect(response.status).toBe(401);
  });

  it("rejects invalid IDs and event bodies", async () => {
    const badId = await fetch(`${base}/api/v1/calendar/events/not-a-uuid`, { headers: dashboardHeaders("usr-alice") });
    expect(badId.status).toBe(400);
    const malformedId = await fetch(`${base}/api/v1/calendar/events/%ZZ`, { headers: dashboardHeaders("usr-alice") });
    expect(malformedId.status).toBe(400);
    const badBody = await fetch(`${base}/api/v1/calendar/events`, {
      method: "POST",
      headers: dashboardHeaders("usr-alice", { "content-type": "application/json" }),
      body: JSON.stringify({ title: "", startTime: "not-a-date", endTime: "not-a-date" }),
    });
    expect(badBody.status).toBe(400);
  });

  it("does not list another owner's overlapping event", async () => {
    const event = await createOwnedEvent("usr-alice", "Alice private event");
    const response = await fetch(`${base}/api/v1/calendar/events?from=2026-09-01&to=2026-09-02`, {
      headers: dashboardHeaders("usr-bob"),
    });
    expect(response.status).toBe(200);
    expect((await response.json() as { events: { id: string }[] }).events).not.toContainEqual(expect.objectContaining({ id: event.id }));
  });

  it("returns 404 when another owner gets an event", async () => {
    const event = await createOwnedEvent("usr-alice", "Alice get-only event");
    const response = await fetch(`${base}/api/v1/calendar/events/${event.id}`, { headers: dashboardHeaders("usr-bob") });
    expect(response.status).toBe(404);
  });

  it("returns 404 without modifying an event when another owner patches it", async () => {
    const event = await createOwnedEvent("usr-alice", "Before foreign patch");
    const patch = await fetch(`${base}/api/v1/calendar/events/${event.id}`, {
      method: "PATCH",
      headers: dashboardHeaders("usr-bob", { "content-type": "application/json" }),
      body: JSON.stringify({ title: "Foreign patch" }),
    });
    expect(patch.status).toBe(404);
    const ownerGet = await fetch(`${base}/api/v1/calendar/events/${event.id}`, { headers: dashboardHeaders("usr-alice") });
    expect((await ownerGet.json() as { title: string }).title).toBe("Before foreign patch");
  });

  it("returns 404 without deleting an event when another owner deletes it", async () => {
    const event = await createOwnedEvent("usr-alice", "Alice delete-only event");
    const deletion = await fetch(`${base}/api/v1/calendar/events/${event.id}`, {
      method: "DELETE",
      headers: dashboardHeaders("usr-bob"),
    });
    expect(deletion.status).toBe(404);
    const ownerGet = await fetch(`${base}/api/v1/calendar/events/${event.id}`, { headers: dashboardHeaders("usr-alice") });
    expect(ownerGet.status).toBe(200);
  });

  it("stops its heartbeat, SQLite engine, and Bun server in shutdown order", async () => {
    const handle = await createServer();
    const engineClose = spyOn(handle.engine, "close").mockImplementation(() => {
      shutdownOrder.push("engine");
    });
    const serverStop = spyOn(handle.server, "stop").mockImplementation(async () => {
      shutdownOrder.push("server");
    });

    shutdownOrder.length = 0;
    handle.close();

    expect(stopHeartbeat).toHaveBeenCalled();
    expect(engineClose).toHaveBeenCalled();
    expect(serverStop).toHaveBeenCalled();
    expect(shutdownOrder).toEqual(["heartbeat", "engine", "server"]);
  });
});
