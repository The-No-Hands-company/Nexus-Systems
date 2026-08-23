import { startHeartbeat } from "./cloud";
import { CalendarEngine } from "./calendar-engine";

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
  const engine = new CalendarEngine(dbPath);

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

      // List events by date range
      if (req.method === "GET" && p === "/api/v1/calendar/events") {
        const from = url.searchParams.get("from") || new Date().toISOString().slice(0, 10);
        const to = url.searchParams.get("to") || new Date(Date.now() + 30 * 86400000).toISOString().slice(0, 10);
        return json({ events: engine.listEvents(from, to) });
      }

      // Create event
      if (req.method === "POST" && p === "/api/v1/calendar/events") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.title || !b.startTime || !b.endTime)
          return json({ error: "title, startTime, endTime required" }, 400);
        return json(engine.createEvent({
          title: b.title as string,
          description: b.description as string | undefined,
          location: b.location as string | undefined,
          startTime: b.startTime as string,
          endTime: b.endTime as string,
          allDay: b.allDay as boolean | undefined,
          recurrence: b.recurrence as string | undefined,
        }), 201);
      }

      // Get / update / delete single event
      const evMatch = p.match(/^\/api\/v1\/calendar\/events\/([^/]+)$/);
      if (evMatch) {
        const id = decodeURIComponent(evMatch[1]!);
        if (req.method === "GET") {
          const ev = engine.getEvent(id);
          return ev ? json(ev) : json({ error: "not found" }, 404);
        }
        if (req.method === "PATCH") {
          const b = await req.json().catch(() => ({})) as Record<string, unknown>;
          const updated = engine.updateEvent(id, b as Partial<typeof engine extends never ? never : ReturnType<CalendarEngine["getEvent"]> & Record<string, never>>);
          return updated ? json(updated) : json({ error: "not found" }, 404);
        }
        if (req.method === "DELETE") {
          return engine.deleteEvent(id) ? json({ deleted: true }) : json({ error: "not found" }, 404);
        }
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-calendar] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, close: () => { stopHeartbeat(); server.stop(); } };
}
