/**
 * The private hop from Dashboard to Nexus-Calendar.
 *
 * Dashboard is the authenticated front door for the canonical `/calendar`
 * route. Calendar itself is loopback-only and has no session of its own, so it
 * has to be told who is asking — and it must be able to tell the difference
 * between Dashboard saying so and a browser claiming it.
 *
 * Three properties make that work, and each is asserted by a test:
 *
 *   1. Authentication happens before the upstream is contacted at all.
 *   2. Any inbound `x-nexus-subject` / `x-nexus-dashboard-secret` is discarded.
 *      Without this, the identity header would be worth exactly as much as the
 *      client's willingness to type it.
 *   3. The subject Auth resolved is attached, together with a deployment secret
 *      the browser never sees, which is what Calendar checks before believing
 *      the subject.
 *
 * Modelled on proxyToMail in server.ts: allow-list the paths, allow-list the
 * methods, forward only what is needed, and return a stable envelope on
 * failure. The Calendar route previously did none of those and forwarded no
 * identity at all, which is why every signed-in user shared one calendar.
 */

import { callerIdentity } from "./auth";

/**
 * Read per call rather than captured at module load, so a test can point this
 * somewhere else without depending on import order.
 */
function calendarUrl(): string {
  return (process.env.NEXUS_CALENDAR_URL || "http://127.0.0.1:3068").replace(/\/+$/, "");
}

/** Exactly the Calendar API surface Dashboard is willing to expose. */
const ALLOWED_PATHS = [
  /^events$/,
  /^events\/[A-Za-z0-9._~-]+$/,
];

const ALLOWED_METHODS = new Set(["GET", "POST", "PATCH", "DELETE"]);

function json(body: unknown, status: number): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      // Someone's schedule is not cacheable by anything shared, and a URL
      // carrying an event id should not travel in a Referer.
      "cache-control": "no-store",
      "referrer-policy": "no-referrer",
      "x-content-type-options": "nosniff",
    },
  });
}

/**
 * @param rest   the path after `/ipa/calendar/`, e.g. `events` or `events/<id>`.
 * @param search the original query string, `?from=…` included.
 */
export async function proxyCalendarApi(req: Request, rest: string, search: string): Promise<Response> {
  // (1) Identity first. Nothing reaches Calendar on behalf of a caller we have
  // not established, so an unauthenticated request cannot even be used to probe
  // whether the service is up.
  const who = await callerIdentity(req);
  if (!who) return json({ error: "not_authenticated" }, 401);

  if (!ALLOWED_PATHS.some((re) => re.test(rest))) return json({ error: "not_found" }, 404);
  if (!ALLOWED_METHODS.has(req.method)) return json({ error: "method_not_allowed" }, 405);

  const secret = process.env.NEXUS_CALENDAR_DASHBOARD_SECRET;
  if (!secret) {
    // Failing closed rather than sending an unauthenticated hop: without the
    // secret Calendar will refuse the subject anyway, and a 503 says which
    // side is misconfigured.
    console.error("[dashboard] NEXUS_CALENDAR_DASHBOARD_SECRET is not set; calendar hop disabled");
    return json({ error: "calendar_unavailable" }, 503);
  }

  // (2) and (3). A fresh Headers, not a copy of the request's — copying would
  // carry the browser's cookie and any header it chose to send, and the
  // interesting ones here are exactly the two it must not control.
  const headers = new Headers();
  headers.set("content-type", req.headers.get("content-type") ?? "application/json");
  headers.set("x-nexus-subject", who.subject);
  headers.set("x-nexus-dashboard-secret", secret);

  try {
    const upstream = await fetch(`${calendarUrl()}/api/v1/calendar/${rest}${search}`, {
      method: req.method,
      headers,
      body: req.method === "POST" || req.method === "PATCH" ? await req.text() : undefined,
      signal: AbortSignal.timeout(5000),
    });

    // A 5xx is the upstream's internal problem and its body describes internal
    // state — a stack, a driver message, a path. Those are for the operator's
    // logs, not for a browser, so they collapse into the same envelope an
    // unreachable service produces.
    //
    // 4xx is the opposite: Calendar's own answer to this caller. "endTime must
    // be after startTime" and "not_found" are written for the client and are
    // the whole reason the status is relayed rather than flattened.
    if (upstream.status >= 500) {
      console.error(`[dashboard] calendar upstream ${upstream.status} for ${req.method} ${rest}`);
      return json({ error: "calendar_unavailable" }, 503);
    }

    // Only the body and a content type cross back; nothing else from an
    // internal service belongs in a browser response.
    return new Response(upstream.body, {
      status: upstream.status,
      headers: {
        "content-type": upstream.headers.get("content-type") ?? "application/json",
        "cache-control": "no-store",
        "referrer-policy": "no-referrer",
        "x-content-type-options": "nosniff",
      },
    });
  } catch {
    // Unreachable, timed out, or refused. One stable envelope either way: which
    // of those it was is an operational detail, not something to hand a client.
    return json({ error: "calendar_unavailable" }, 503);
  }
}
