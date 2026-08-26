import { describe, it, expect, beforeAll, afterAll } from "bun:test";
import { createServer } from "../src/server";

const SECRET = "t".repeat(48);
const ALICE = "usr-alice";
const BOB = "usr-bob";

describe("nexus-calendar HTTP", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    process.env.NEXUS_CALENDAR_DB = ":memory:";
    process.env.NEXUS_CALENDAR_DASHBOARD_SECRET = SECRET;
    process.env.PORT = "0";
    handle = await createServer();
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => handle.close());

  /** A request on the trusted Dashboard hop, as the named subject. */
  function as(subject: string, init: RequestInit = {}): RequestInit {
    return {
      ...init,
      headers: {
        "content-type": "application/json",
        "x-nexus-subject": subject,
        "x-nexus-dashboard-secret": SECRET,
        ...(init.headers as Record<string, string> | undefined),
      },
    };
  }

  async function createFor(subject: string, body: Record<string, unknown>) {
    const res = await fetch(`${base}/api/v1/calendar/events`, as(subject, {
      method: "POST",
      body: JSON.stringify(body),
    }));
    return { res, json: await res.json() as Record<string, unknown> };
  }

  const SEPT = { startTime: "2026-09-01T10:00:00.000Z", endTime: "2026-09-01T11:00:00.000Z" };

  describe("public surface", () => {
    it("serves /health without identity", async () => {
      expect((await fetch(`${base}/health`)).status).toBe(200);
    });

    it("serves /api/v1/status without identity", async () => {
      expect((await fetch(`${base}/api/v1/status`)).status).toBe(200);
    });

    it("does not leak event data through status", async () => {
      const body = await (await fetch(`${base}/api/v1/status`)).text();
      expect(body).not.toContain("owner");
    });
  });

  describe("everything else needs a trusted identity", () => {
    it.each([
      ["GET", "/api/v1/calendar/events"],
      ["POST", "/api/v1/calendar/events"],
      ["GET", "/api/v1/calendar/events/anything"],
      ["PATCH", "/api/v1/calendar/events/anything"],
      ["DELETE", "/api/v1/calendar/events/anything"],
    ])("401s %s %s with no identity", async (method, path) => {
      const res = await fetch(`${base}${path}`, {
        method,
        headers: { "content-type": "application/json" },
        ...(method === "POST" || method === "PATCH" ? { body: "{}" } : {}),
      });
      expect(res.status).toBe(401);
    });

    it("401s when the subject arrives without the hop secret", async () => {
      // The browser-forgeable header on its own means nothing.
      const res = await fetch(`${base}/api/v1/calendar/events`, {
        headers: { "x-nexus-subject": ALICE },
      });
      expect(res.status).toBe(401);
    });
  });

  describe("owner-scoped CRUD", () => {
    it("creates and reads back its own event", async () => {
      const { res, json } = await createFor(ALICE, { title: "Mine", ...SEPT });
      expect(res.status).toBe(201);
      expect(json.ownerSubject).toBe(ALICE);

      const get = await fetch(`${base}/api/v1/calendar/events/${json.id}`, as(ALICE));
      expect(get.status).toBe(200);
    });

    it("stamps the owner from the hop, not the body", async () => {
      const { res } = await createFor(ALICE, { title: "x", ...SEPT, ownerSubject: BOB });
      // ownerSubject is not an accepted field, so this is a 400 rather than a
      // silent reassignment.
      expect(res.status).toBe(400);
    });

    it("hides another user's event behind a 404", async () => {
      const { json } = await createFor(ALICE, { title: "Alice only", ...SEPT });
      const asBob = await fetch(`${base}/api/v1/calendar/events/${json.id}`, as(BOB));
      expect(asBob.status).toBe(404);
    });

    it("keeps another user's event out of the listing", async () => {
      await createFor(ALICE, { title: "Alice only 2", ...SEPT });
      const res = await fetch(`${base}/api/v1/calendar/events?from=2026-09-01&to=2026-09-30`, as(BOB));
      const body = await res.json() as { events: unknown[] };
      expect(body.events).toEqual([]);
    });

    it("refuses another user's patch and delete", async () => {
      const { json } = await createFor(ALICE, { title: "Untouchable", ...SEPT });
      const patch = await fetch(`${base}/api/v1/calendar/events/${json.id}`, as(BOB, {
        method: "PATCH", body: JSON.stringify({ title: "hijacked" }),
      }));
      expect(patch.status).toBe(404);
      const del = await fetch(`${base}/api/v1/calendar/events/${json.id}`, as(BOB, { method: "DELETE" }));
      expect(del.status).toBe(404);
    });

    it("patches and deletes its own event", async () => {
      const { json } = await createFor(ALICE, { title: "Before", ...SEPT });
      const patch = await fetch(`${base}/api/v1/calendar/events/${json.id}`, as(ALICE, {
        method: "PATCH", body: JSON.stringify({ title: "After" }),
      }));
      expect(patch.status).toBe(200);
      expect((await patch.json() as { title: string }).title).toBe("After");

      const del = await fetch(`${base}/api/v1/calendar/events/${json.id}`, as(ALICE, { method: "DELETE" }));
      expect(del.status).toBe(200);
      expect((await fetch(`${base}/api/v1/calendar/events/${json.id}`, as(ALICE))).status).toBe(404);
    });

    it("404s an unknown id rather than disclosing that it is unknown", async () => {
      const res = await fetch(`${base}/api/v1/calendar/events/does-not-exist`, as(ALICE));
      expect(res.status).toBe(404);
    });
  });

  describe("input validation", () => {
    it.each([
      ["a missing title", { ...SEPT }],
      ["a blank title", { title: "   ", ...SEPT }],
      ["an unknown field", { title: "x", ...SEPT, ownerSubject: "usr-x" }],
      ["a malformed timestamp", { title: "x", startTime: "yesterday", endTime: "2026-09-01T11:00:00.000Z" }],
      ["an end before the start", { title: "x", startTime: "2026-09-01T11:00:00.000Z", endTime: "2026-09-01T10:00:00.000Z" }],
    ])("400s %s", async (_label, body) => {
      const { res } = await createFor(ALICE, body as Record<string, unknown>);
      expect(res.status).toBe(400);
    });

    it("400s a malformed range", async () => {
      const res = await fetch(`${base}/api/v1/calendar/events?from=nonsense&to=2026-09-30`, as(ALICE));
      expect(res.status).toBe(400);
    });

    it("400s an unbounded range", async () => {
      const res = await fetch(`${base}/api/v1/calendar/events?from=2000-01-01&to=2030-01-01`, as(ALICE));
      expect(res.status).toBe(400);
    });

    it("400s a body that is not JSON", async () => {
      const res = await fetch(`${base}/api/v1/calendar/events`, as(ALICE, { method: "POST", body: "not json" }));
      expect(res.status).toBe(400);
    });
  });

  describe("listing uses overlap", () => {
    it("returns a holiday that started before the window", async () => {
      await createFor(BOB, {
        title: "Bob's two-week holiday",
        startTime: "2026-10-20T00:00:00.000Z",
        endTime: "2026-11-03T00:00:00.000Z",
      });
      const res = await fetch(`${base}/api/v1/calendar/events?from=2026-11-01&to=2026-11-10`, as(BOB));
      const body = await res.json() as { events: { title: string }[] };
      expect(body.events.map((e) => e.title)).toContain("Bob's two-week holiday");
    });
  });
});
