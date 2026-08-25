import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createHash } from "node:crypto";
import { createServer } from "../src/server";

const SECRET = "p".repeat(48);
const ALICE = "usr-alice";
const BOB = "usr-bob";

describe("nexus-calendar public event sharing", () => {
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

  function as(subject: string, init: RequestInit = {}): RequestInit {
    return {
      ...init,
      headers: {
        "x-nexus-subject": subject,
        "x-nexus-dashboard-secret": SECRET,
        ...(init.headers as Record<string, string> | undefined),
      },
    };
  }

  async function createEvent(): Promise<{ id: string }> {
    const response = await fetch(`${base}/api/v1/calendar/events`, as(ALICE, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        title: "Public planning",
        description: "Only the approved fields are public.",
        location: "Nexus HQ",
        startTime: "2026-09-01T10:00:00.000Z",
        endTime: "2026-09-01T11:00:00.000Z",
        allDay: false,
      }),
    }));
    expect(response.status).toBe(201);
    return response.json() as Promise<{ id: string }>;
  }

  async function createPublicShare(eventId: string, subject = ALICE): Promise<{ token: string; publicPath: string }> {
    const response = await fetch(`${base}/api/v1/calendar/events/${eventId}/public-share`, as(subject, { method: "POST" }));
    expect(response.status).toBe(201);
    return response.json() as Promise<{ token: string; publicPath: string }>;
  }

  it("creates a 256-bit bearer token, stores only its digest, and returns the filtered event", async () => {
    const event = await createEvent();
    const share = await createPublicShare(event.id);

    expect(Buffer.from(share.token, "base64url")).toHaveLength(32);
    expect(share.publicPath).toBe(`/api/v1/calendar/public/${share.token}`);
    expect(handle.engine.db.prepare("SELECT token_digest FROM public_event_shares WHERE event_id = ?").get(event.id)).toEqual({
      token_digest: createHash("sha256").update(share.token).digest("hex"),
    });
    const columns = handle.engine.db.query("PRAGMA table_info(public_event_shares)").all() as { name: string }[];
    expect(columns.map((column) => column.name)).not.toContain("token");

    const response = await fetch(`${base}${share.publicPath}`);
    expect(response.status).toBe(200);
    expect(await response.json()).toEqual({
      title: "Public planning",
      startTime: "2026-09-01T10:00:00.000Z",
      endTime: "2026-09-01T11:00:00.000Z",
      allDay: false,
      location: "Nexus HQ",
      description: "Only the approved fields are public.",
    });
  });

  it("replaces a public link and makes its prior token unreachable", async () => {
    const event = await createEvent();
    const first = await createPublicShare(event.id);
    const second = await createPublicShare(event.id);

    expect(second.token).not.toBe(first.token);
    expect((await fetch(`${base}${first.publicPath}`)).status).toBe(404);
    expect((await fetch(`${base}${second.publicPath}`)).status).toBe(200);
  });

  it("only lets an owner create or revoke a public link, and revocation makes it unreachable", async () => {
    const event = await createEvent();
    const forbidden = await fetch(`${base}/api/v1/calendar/events/${event.id}/public-share`, as(BOB, { method: "POST" }));
    expect(forbidden.status).toBe(404);

    const share = await createPublicShare(event.id);
    const forbiddenRevoke = await fetch(`${base}/api/v1/calendar/events/${event.id}/public-share`, as(BOB, { method: "DELETE" }));
    expect(forbiddenRevoke.status).toBe(404);
    const revoke = await fetch(`${base}/api/v1/calendar/events/${event.id}/public-share`, as(ALICE, { method: "DELETE" }));
    expect(revoke.status).toBe(200);
    expect((await fetch(`${base}${share.publicPath}`)).status).toBe(404);
  });

  it("returns 404 for malformed or unknown tokens and 405 for non-GET public reads", async () => {
    const event = await createEvent();
    const share = await createPublicShare(event.id);

    expect((await fetch(`${base}/api/v1/calendar/public/not-a-token`)).status).toBe(404);
    expect((await fetch(`${base}/api/v1/calendar/public/${"a".repeat(43)}`)).status).toBe(404);
    expect((await fetch(`${base}${share.publicPath}`, { method: "POST" })).status).toBe(405);
  });
});
