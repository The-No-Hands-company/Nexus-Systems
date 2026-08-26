import { startHeartbeat } from "./cloud";
import { CalendarEngine } from "./calendar-engine";
import { resolveCaller } from "./auth";
import { parseEventCreate, parseEventPatch, parseRange } from "./validation";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "content-security-policy": "frame-ancestors 'self' https://app.tnhc.dev",
      "x-content-type-options": "nosniff",
      // Event titles and locations are personal. Nothing here should be
      // reachable from a shared cache, and a URL carrying an event id should
      // not travel to third parties in a Referer.
      "cache-control": "no-store",
      "referrer-policy": "no-referrer",
    },
  });
}

/**
 * The one shape used for "you may not see this".
 *
 * An event that does not exist and an event belonging to somebody else are the
 * same answer, so the API cannot be walked to discover which ids are real.
 */
const notFound = () => json({ error: "not_found" }, 404);

export async function createServer() {
  const port = Number(process.env.PORT || "3068");
  // NEXUS_NEXUS_CALENDAR_BASE_URL is the doubled-prefix name the scaffolding
  // generated and deploy.sh has been setting. Both are read so renaming it in
  // one place cannot silently fall back to localhost and deregister the service.
  const baseUrl =
    process.env.NEXUS_CALENDAR_BASE_URL ||
    process.env.NEXUS_NEXUS_CALENDAR_BASE_URL ||
    `http://localhost:${port}`;
  const startedAt = Date.now();

  // Persistent SQLite — survives restarts, unlike :memory:
  const dbPath = process.env.NEXUS_CALENDAR_DB || "data/calendar.sqlite";
  // Only consulted when migrating events that predate ownership. The engine
  // refuses to start rather than invent an owner for them.
  const legacyOwnerSubject = process.env.NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT;
  const engine = new CalendarEngine(dbPath, legacyOwnerSubject ? { legacyOwnerSubject } : {});

  const server = Bun.serve({
    port,
    // Loopback only. Identity arrives as a header from a trusted front door, so
    // the service must never be directly reachable from a network where anyone
    // could set that header themselves.
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname;

      // ── Public: liveness and capability advertisement only. Neither reads
      //    the events table, so neither needs an identity.
      if (req.method === "GET" && p === "/health") {
        return json({
          service: "nexus-calendar",
          status: "ok",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
        });
      }
      if (req.method === "GET" && p === "/api/v1/status") {
        return json({ service: "nexus-calendar", status: "ready", capabilities: ["calendar", "events"] });
      }

      // ── Everything below touches user data.
      const caller = await resolveCaller(req);
      if (!caller) return json({ error: "not_authenticated" }, 401);

      if (p === "/api/v1/calendar/events") {
        if (req.method === "GET") {
          const range = parseRange(url);
          if (!range.ok) return json({ error: range.error }, 400);
          return json({ events: engine.listEvents(caller.subject, range.value) });
        }

        if (req.method === "POST") {
          const body = await req.json().catch(() => undefined);
          if (body === undefined) return json({ error: "body must be valid JSON" }, 400);
          const parsed = parseEventCreate(body);
          if (!parsed.ok) return json({ error: parsed.error }, 400);
          // The owner is the caller. There is no path from request data to this
          // argument, which is the property that makes the calendar private.
          return json(engine.createEvent(caller.subject, parsed.value), 201);
        }

        return json({ error: "method_not_allowed" }, 405);
      }

      const evMatch = p.match(/^\/api\/v1\/calendar\/events\/([^/]+)$/);
      if (evMatch) {
        const id = decodeURIComponent(evMatch[1]!);

        if (req.method === "GET") {
          const ev = engine.getEvent(caller.subject, id);
          return ev ? json(ev) : notFound();
        }

        if (req.method === "PATCH") {
          const body = await req.json().catch(() => undefined);
          if (body === undefined) return json({ error: "body must be valid JSON" }, 400);
          const parsed = parseEventPatch(body);
          if (!parsed.ok) return json({ error: parsed.error }, 400);

          // Authorization before validation-against-stored-state, so a
          // non-owner learns nothing about the event from the error it gets.
          const existing = engine.getEvent(caller.subject, id);
          if (!existing) return notFound();

          // A patch may move one end of the interval; the pair still has to
          // make sense once merged with what is stored.
          const startTime = parsed.value.startTime ?? existing.startTime;
          const endTime = parsed.value.endTime ?? existing.endTime;
          if (new Date(endTime).getTime() <= new Date(startTime).getTime()) {
            return json({ error: "endTime must be after startTime" }, 400);
          }

          const updated = engine.updateEvent(caller.subject, id, parsed.value);
          return updated ? json(updated) : notFound();
        }

        if (req.method === "DELETE") {
          return engine.deleteEvent(caller.subject, id) ? json({ deleted: true }) : notFound();
        }

        return json({ error: "method_not_allowed" }, 405);
      }

      return notFound();
    },
  });

  console.log(`[nexus-calendar] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    close: () => {
      stopHeartbeat();
      engine.close();
      server.stop();
    },
  };
}
