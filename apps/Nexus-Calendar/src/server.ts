import { startHeartbeat } from "./cloud";
import { CalendarEngine, type EventCreate } from "./calendar-engine";
import { resolveCaller } from "./auth";
import { isEventId, parseEventCreate, parseEventPatch, parseEventShare, parseRange } from "./validation";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "content-security-policy": "frame-ancestors 'self' https://app.tnhc.dev",
      "x-content-type-options": "nosniff",
    },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3068");
  const baseUrl = process.env.NEXUS_NEXUS_CALENDAR_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  // Persistent SQLite — survives restarts, unlike :memory:
  const dbPath = process.env.NEXUS_CALENDAR_DB || "data/calendar.sqlite";
  const legacyOwnerSubject = process.env.NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT;
  const engine = new CalendarEngine(
    dbPath,
    legacyOwnerSubject === undefined ? {} : { legacyOwnerSubject },
  );

  const server = Bun.serve({
    port,
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname;

      if (req.method === "GET" && p === "/health")
        return json({ service: "nexus-calendar", status: "ok", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });

      if (req.method === "GET" && p === "/api/v1/status")
        return json({ service: "nexus-calendar", status: "ready", capabilities: ["calendar", "events"] });

      const isEventsRoute = p === "/api/v1/calendar/events"
        || /^\/api\/v1\/calendar\/events\/[^/]+(?:\/shares(?:\/[^/]+)?)?$/.test(p);
      const caller = isEventsRoute ? await resolveCaller(req) : null;
      if (isEventsRoute && !caller) return json({ error: "not authenticated" }, 401);

      // List events by date range
      if (req.method === "GET" && p === "/api/v1/calendar/events") {
        const range = parseRange(url);
        if (!range.ok) return json({ error: range.error }, 400);
        return json({ events: engine.listEvents(caller!.subject, range.value) });
      }

      // Create event
      if (req.method === "POST" && p === "/api/v1/calendar/events") {
        const input = parseEventCreate(await req.json().catch(() => null));
        if (!input.ok) return json({ error: input.error }, 400);
        return json(engine.createEvent(caller!.subject, input.value), 201);
      }

      // Manage explicit shares. Every engine method independently verifies ownership.
      const shareMatch = p.match(/^\/api\/v1\/calendar\/events\/([^/]+)\/shares(?:\/([^/]+))?$/);
      if (shareMatch) {
        let id: string;
        try {
          id = decodeURIComponent(shareMatch[1]!);
        } catch {
          return json({ error: "invalid event id" }, 400);
        }
        if (!isEventId(id)) return json({ error: "invalid event id" }, 400);

        if (req.method === "GET" && shareMatch[2] === undefined) {
          const shares = engine.listShares(caller!.subject, id);
          return shares ? json({ shares }) : json({ error: "not found" }, 404);
        }

        if ((req.method === "PUT" || req.method === "DELETE") && shareMatch[2] !== undefined) {
          let subject: string;
          try {
            subject = decodeURIComponent(shareMatch[2]!).trim();
          } catch {
            return json({ error: "invalid share subject" }, 400);
          }
          if (!subject) return json({ error: "invalid share subject" }, 400);
          if (engine.listShares(caller!.subject, id) === undefined) return json({ error: "not found" }, 404);
          if (req.method === "DELETE") return json({ deleted: engine.deleteShare(caller!.subject, id, subject) });

          const permission = parseEventShare(await req.json().catch(() => null));
          if (!permission.ok) return json({ error: permission.error }, 400);
          if (subject === caller!.subject) return json({ error: "cannot share an event with its owner" }, 400);
          const share = engine.upsertShare(caller!.subject, id, subject, permission.value);
          return share ? json(share) : json({ error: "not found" }, 404);
        }
      }

      // Get / update / delete single event
      const evMatch = p.match(/^\/api\/v1\/calendar\/events\/([^/]+)$/);
      if (evMatch) {
        let id: string;
        try {
          id = decodeURIComponent(evMatch[1]!);
        } catch {
          return json({ error: "invalid event id" }, 400);
        }
        if (!isEventId(id)) return json({ error: "invalid event id" }, 400);
        if (req.method === "GET") {
          const ev = engine.getEvent(caller!.subject, id);
          return ev ? json(ev) : json({ error: "not found" }, 404);
        }
        if (req.method === "PATCH") {
          const patch = parseEventPatch(await req.json().catch(() => null));
          if (!patch.ok) return json({ error: patch.error }, 400);
          const existing = engine.getEvent(caller!.subject, id);
          if (!existing) return json({ error: "not found" }, 404);
          const validMergedEvent = parseEventCreate({
            title: existing.title,
            description: existing.description,
            location: existing.location,
            startTime: existing.startTime,
            endTime: existing.endTime,
            allDay: existing.allDay,
            recurrence: existing.recurrence,
            ...patch.value,
          });
          if (!validMergedEvent.ok) return json({ error: validMergedEvent.error }, 400);
          const updated = engine.updateEvent(caller!.subject, id, patch.value as Partial<EventCreate>);
          return updated ? json(updated) : json({ error: "not found" }, 404);
        }
        if (req.method === "DELETE") {
          return engine.deleteEvent(caller!.subject, id) ? json({ deleted: true }) : json({ error: "not found" }, 404);
        }
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-calendar] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    engine,
    close: () => {
      stopHeartbeat();
      engine.close();
      server.stop();
    },
  };
}
