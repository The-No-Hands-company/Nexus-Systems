import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

const SECRET = "t".repeat(48);
const ALICE = "usr-alice";
const BOB = "usr-bob";
const CAROL = "usr-carol";
const DAVE = "usr-dave";
const SEPT = { startTime: "2026-09-01T10:00:00.000Z", endTime: "2026-09-01T11:00:00.000Z" };

describe("nexus-calendar internal event sharing", () => {
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
        "content-type": "application/json",
        "x-nexus-subject": subject,
        "x-nexus-dashboard-secret": SECRET,
        ...(init.headers as Record<string, string> | undefined),
      },
    };
  }

  async function createFor(subject: string, title: string): Promise<Record<string, unknown>> {
    const response = await fetch(`${base}/api/v1/calendar/events`, as(subject, {
      method: "POST",
      body: JSON.stringify({ title, ...SEPT }),
    }));
    expect(response.status).toBe(201);
    return await response.json() as Record<string, unknown>;
  }

  async function grant(eventId: string, subject: string, permission: "viewer" | "editor"): Promise<Response> {
    return fetch(`${base}/api/v1/calendar/events/${eventId}/shares/${subject}`, as(ALICE, {
      method: "PUT",
      body: JSON.stringify({ permission }),
    }));
  }

  it("requires a trusted caller for share routes", async () => {
    const event = await createFor(ALICE, "Unauthenticated shares");
    const response = await fetch(`${base}/api/v1/calendar/events/${event.id}/shares`);
    expect(response.status).toBe(401);
  });

  it("lets only an owner create, list, update, and revoke explicit grants", async () => {
    const event = await createFor(ALICE, "Owner grants");
    const eventId = event.id as string;

    expect((await grant(eventId, BOB, "viewer")).status).toBe(200);
    expect((await grant(eventId, CAROL, "editor")).status).toBe(200);

    const ownerShares = await fetch(`${base}/api/v1/calendar/events/${eventId}/shares`, as(ALICE));
    expect(ownerShares.status).toBe(200);
    expect(await ownerShares.json()).toMatchObject({
      shares: [
        { eventId, subject: BOB, permission: "viewer", createdBy: ALICE },
        { eventId, subject: CAROL, permission: "editor", createdBy: ALICE },
      ],
    });

    const viewerShares = await fetch(`${base}/api/v1/calendar/events/${eventId}/shares`, as(BOB));
    expect(viewerShares.status).toBe(404);
    const editorGrant = await fetch(`${base}/api/v1/calendar/events/${eventId}/shares/${DAVE}`, as(CAROL, {
      method: "PUT",
      body: JSON.stringify({ permission: "viewer" }),
    }));
    expect(editorGrant.status).toBe(404);
    const strangerRevoke = await fetch(`${base}/api/v1/calendar/events/${eventId}/shares/${BOB}`, as(DAVE, { method: "DELETE" }));
    expect(strangerRevoke.status).toBe(404);

    const revoke = await fetch(`${base}/api/v1/calendar/events/${eventId}/shares/${BOB}`, as(ALICE, { method: "DELETE" }));
    expect(revoke.status).toBe(200);
    const revokedViewer = await fetch(`${base}/api/v1/calendar/events/${eventId}`, as(BOB));
    expect(revokedViewer.status).toBe(404);
    const repeatedRevoke = await fetch(`${base}/api/v1/calendar/events/${eventId}/shares/${BOB}`, as(ALICE, { method: "DELETE" }));
    expect(repeatedRevoke.status).toBe(200);
  });

  it("applies the viewer, editor, owner, and stranger permission matrix", async () => {
    const event = await createFor(ALICE, "Permission matrix");
    const eventId = event.id as string;
    await grant(eventId, BOB, "viewer");
    await grant(eventId, CAROL, "editor");

    const viewerRead = await fetch(`${base}/api/v1/calendar/events/${eventId}`, as(BOB));
    expect(viewerRead.status).toBe(200);
    expect(await viewerRead.json()).toMatchObject({ access: "viewer" });
    const viewerPatch = await fetch(`${base}/api/v1/calendar/events/${eventId}`, as(BOB, {
      method: "PATCH", body: JSON.stringify({ title: "Viewer edit" }),
    }));
    expect(viewerPatch.status).toBe(404);
    const viewerDelete = await fetch(`${base}/api/v1/calendar/events/${eventId}`, as(BOB, { method: "DELETE" }));
    expect(viewerDelete.status).toBe(404);

    const editorRead = await fetch(`${base}/api/v1/calendar/events/${eventId}`, as(CAROL));
    expect(editorRead.status).toBe(200);
    expect(await editorRead.json()).toMatchObject({ access: "editor" });
    const editorPatch = await fetch(`${base}/api/v1/calendar/events/${eventId}`, as(CAROL, {
      method: "PATCH", body: JSON.stringify({ title: "Editor edit" }),
    }));
    expect(editorPatch.status).toBe(200);
    const editorDelete = await fetch(`${base}/api/v1/calendar/events/${eventId}`, as(CAROL, { method: "DELETE" }));
    expect(editorDelete.status).toBe(404);

    const ownerRead = await fetch(`${base}/api/v1/calendar/events/${eventId}`, as(ALICE));
    expect(ownerRead.status).toBe(200);
    const ownerDelete = await fetch(`${base}/api/v1/calendar/events/${eventId}`, as(ALICE, { method: "DELETE" }));
    expect(ownerDelete.status).toBe(200);

    const strangerRead = await fetch(`${base}/api/v1/calendar/events/${eventId}`, as(DAVE));
    expect(strangerRead.status).toBe(404);
  });

  it("lists owned and shared events exactly once after an idempotent upsert", async () => {
    const owned = await createFor(BOB, "Bob owned");
    const shared = await createFor(ALICE, "Alice shared");
    const sharedId = shared.id as string;

    await grant(sharedId, BOB, "viewer");
    await grant(sharedId, BOB, "editor");

    const response = await fetch(`${base}/api/v1/calendar/events?from=2026-09-01&to=2026-09-02`, as(BOB));
    expect(response.status).toBe(200);
    const body = await response.json() as { events: { id: string; access: string }[] };
    expect(body.events.filter((event) => event.id === owned.id)).toHaveLength(1);
    expect(body.events.filter((event) => event.id === sharedId)).toMatchObject([{ id: sharedId, access: "editor" }]);
  });

  it("rejects owner self-sharing and malformed grant bodies", async () => {
    const event = await createFor(ALICE, "Grant validation");
    const eventId = event.id as string;

    const selfShare = await fetch(`${base}/api/v1/calendar/events/${eventId}/shares/${ALICE}`, as(ALICE, {
      method: "PUT", body: JSON.stringify({ permission: "viewer" }),
    }));
    expect(selfShare.status).toBe(400);
    const unknownField = await fetch(`${base}/api/v1/calendar/events/${eventId}/shares/${BOB}`, as(ALICE, {
      method: "PUT", body: JSON.stringify({ permission: "viewer", extra: true }),
    }));
    expect(unknownField.status).toBe(400);
    const invalidPermission = await fetch(`${base}/api/v1/calendar/events/${eventId}/shares/${BOB}`, as(ALICE, {
      method: "PUT", body: JSON.stringify({ permission: "owner" }),
    }));
    expect(invalidPermission.status).toBe(400);
  });
});
