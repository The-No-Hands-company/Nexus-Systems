import { callerIdentity } from "./auth";

const API_TIMEOUT_MS = 5_000;
const WEB_TIMEOUT_MS = 5_000;
const UNAVAILABLE = { error: "calendar_unavailable" };
const HASHED_ASSET = /[-_.][A-Za-z0-9_-]{8,}\.(?:js|css|woff2?)$/;
const SEGMENT = "[A-Za-z0-9_-]+";
const PUBLIC_TOKEN = "[A-Za-z0-9_-]{43}";

type ApiRoute = {
  path: RegExp;
  methods: readonly string[];
};

// This mirrors Calendar's public HTTP surface. It is deliberately a list of
// resources rather than a prefix: Dashboard must never become a generic
// request tunnel into the loopback Calendar service.
const API_ROUTES: readonly ApiRoute[] = [
  { path: /^events$/, methods: ["GET", "POST"] },
  { path: new RegExp(`^events/${SEGMENT}$`), methods: ["GET", "PATCH", "DELETE"] },
  { path: new RegExp(`^events/${SEGMENT}/shares$`), methods: ["GET"] },
  { path: new RegExp(`^events/${SEGMENT}/shares/${SEGMENT}$`), methods: ["PUT", "DELETE"] },
  { path: new RegExp(`^events/${SEGMENT}/public-share$`), methods: ["POST", "DELETE"] },
  { path: new RegExp(`^public/${PUBLIC_TOKEN}$`), methods: ["GET"] },
];

function apiUrl(): string {
  // NEXUS_CALENDAR_URL is the name deploy.sh has always set for this hop.
  return (process.env.NEXUS_CALENDAR_API_URL || process.env.NEXUS_CALENDAR_URL || "http://127.0.0.1:3068").replace(/\/+$/, "");
}

function webUrl(): string {
  return (process.env.NEXUS_CALENDAR_WEB_URL || "http://127.0.0.1:8092").replace(/\/+$/, "");
}

function unavailable(): Response {
  return Response.json(UNAVAILABLE, { status: 503 });
}

function routeFor(rest: string): ApiRoute | undefined {
  return API_ROUTES.find((route) => route.path.test(rest));
}

function safeRequestHeaders(req: Request): Headers {
  const headers = new Headers();
  for (const name of ["accept", "content-type", "if-match", "if-none-match"] as const) {
    const value = req.headers.get(name);
    if (value) headers.set(name, value);
  }
  return headers;
}

function safeResponseHeaders(upstream: Response): Headers {
  const headers = new Headers();
  for (const name of [
    "content-type",
    "cache-control",
    "etag",
    "last-modified",
    "content-disposition",
    "x-content-type-options",
    "content-security-policy",
    "referrer-policy",
  ] as const) {
    const value = upstream.headers.get(name);
    if (value) headers.set(name, value);
  }
  return headers;
}

function isSafeWebPath(relativePath: string): boolean {
  if (relativePath !== "" && !relativePath.startsWith("/")) return false;
  // A protocol-relative path would make `new URL(path, base)` select an
  // attacker-controlled origin instead of the configured Calendar web host.
  if (relativePath.startsWith("//")) return false;
  if (relativePath.includes("\\") || relativePath.includes("\0")) return false;

  for (const rawSegment of relativePath.split("/")) {
    if (!rawSegment) continue;
    let segment = rawSegment;
    try {
      // A reverse proxy may decode the path one more time than this process.
      // Decode repeatedly so `%252e%252e` cannot turn into traversal only
      // after Dashboard has accepted it. Calendar assets never need encoded
      // path separators, so reject anything that still encodes after this.
      for (let depth = 0; depth < 8 && /%[0-9A-Fa-f]{2}/.test(segment); depth++) {
        segment = decodeURIComponent(segment);
      }
    } catch {
      return false;
    }
    if (
      segment === "." ||
      segment === ".." ||
      segment.includes("/") ||
      segment.includes("\\") ||
      /%[0-9A-Fa-f]{2}/.test(segment)
    ) {
      return false;
    }
  }

  // The private Calendar API belongs exclusively behind /ipa/calendar. A web
  // request that resembles it must not reach the Caddy SPA fallback.
  return relativePath !== "/api" && !relativePath.startsWith("/api/");
}

function isHtml(upstream: Response): boolean {
  return upstream.headers.get("content-type")?.toLowerCase().includes("text/html") ?? false;
}

/**
 * Authenticated, fixed-surface proxy for Calendar's loopback API.
 *
 * Browser credentials and identity headers deliberately never leave this
 * process. Calendar receives only the subject that Auth returned plus the
 * deployment-only hop secret that proves Dashboard supplied it.
 */
export async function proxyCalendarApi(req: Request, rest: string, search: string): Promise<Response> {
  const who = await callerIdentity(req);
  if (!who) return Response.json({ error: "not_authenticated" }, { status: 401 });

  const route = routeFor(rest);
  if (!route) return Response.json({ error: "not_found" }, { status: 404 });
  if (!route.methods.includes(req.method)) {
    return Response.json({ error: "method_not_allowed" }, { status: 405 });
  }

  const secret = process.env.NEXUS_CALENDAR_DASHBOARD_SECRET;
  if (!secret) {
    // Failing closed rather than sending an unauthenticated hop: without the
    // secret Calendar will refuse the subject anyway, and the log says which
    // side is misconfigured.
    console.error("[dashboard] NEXUS_CALENDAR_DASHBOARD_SECRET is not set; calendar hop disabled");
    return unavailable();
  }

  const headers = safeRequestHeaders(req);
  headers.set("x-nexus-subject", who.subject);
  headers.set("x-nexus-dashboard-secret", secret);

  try {
    const upstream = new URL(`/api/v1/calendar/${rest}`, apiUrl());
    upstream.search = search;
    const response = await fetch(upstream, {
      method: req.method,
      headers,
      body: req.method === "GET" || req.method === "HEAD" ? undefined : await req.arrayBuffer(),
      signal: AbortSignal.timeout(API_TIMEOUT_MS),
    });
    // A 5xx is the upstream's internal problem and its body describes internal
    // state — a stack, a driver message, a path. That is for the operator's
    // logs, not a browser, so it collapses into the unreachable envelope. A 4xx
    // is Calendar's own answer to this caller and is relayed as-is.
    if (response.status >= 500) {
      console.error(`[dashboard] calendar upstream ${response.status} for ${req.method} ${rest}`);
      return unavailable();
    }
    const responseHeaders = safeResponseHeaders(response);
    // Someone's schedule is not cacheable by anything shared, and a URL
    // carrying an event id or public token must not travel in a Referer —
    // whatever the upstream happened to send.
    responseHeaders.set("cache-control", "no-store");
    responseHeaders.set("referrer-policy", "no-referrer");
    responseHeaders.set("x-content-type-options", "nosniff");
    return new Response(response.body, { status: response.status, headers: responseHeaders });
  } catch {
    return unavailable();
  }
}

/**
 * Serves Calendar's own web artifact below the Dashboard shell path.
 * Calendar remains responsible for its SPA fallback; Dashboard only supplies
 * the path boundary, cache contract, and explicit shell-context marker.
 */
export async function proxyCalendarWeb(req: Request, relativePath: string): Promise<Response> {
  if (req.method !== "GET" && req.method !== "HEAD") {
    return Response.json({ error: "method_not_allowed" }, { status: 405 });
  }
  if (!isSafeWebPath(relativePath)) return Response.json({ error: "not_found" }, { status: 404 });

  try {
    const upstream = new URL(relativePath || "/", webUrl());
    upstream.search = new URL(req.url).search;
    const response = await fetch(upstream, {
      method: req.method,
      headers: safeRequestHeaders(req),
      signal: AbortSignal.timeout(WEB_TIMEOUT_MS),
    });
    const headers = safeResponseHeaders(response);
    headers.set("x-nexus-shell-context", "proxied-app");
    if (isHtml(response)) {
      headers.set("cache-control", "no-cache, no-store, must-revalidate");
    } else if (HASHED_ASSET.test(relativePath)) {
      headers.set("cache-control", "public, max-age=31536000, immutable");
    }
    return new Response(response.body, { status: response.status, headers });
  } catch {
    return unavailable();
  }
}
