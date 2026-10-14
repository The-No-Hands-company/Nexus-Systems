import { callerIdentity } from "./auth";

const API_TIMEOUT_MS = 5_000;
const WEB_TIMEOUT_MS = 5_000;
const UNAVAILABLE = { error: "project_unavailable" };
const HASHED_ASSET = /[-_.][A-Za-z0-9_-]{8,}\.(?:js|css|woff2?)$/;
const SEGMENT = "[A-Za-z0-9_-]+";

type ApiRoute = {
  path: RegExp;
  methods: readonly string[];
};

// This mirrors Project's public HTTP surface. It is deliberately a list of
// resources rather than a prefix: Dashboard must never become a generic
// request tunnel into the loopback Project service.
const API_ROUTES: readonly ApiRoute[] = [
  { path: /^workspaces$/, methods: ["GET", "POST"] },
  { path: new RegExp(`^workspaces/${SEGMENT}$`), methods: ["PATCH", "DELETE"] },
  { path: new RegExp(`^workspaces/${SEGMENT}/members$`), methods: ["GET"] },
  { path: new RegExp(`^workspaces/${SEGMENT}/members/${SEGMENT}$`), methods: ["PUT", "DELETE"] },
  { path: new RegExp(`^workspaces/${SEGMENT}/projects$`), methods: ["GET", "POST"] },
  { path: new RegExp(`^projects/${SEGMENT}$`), methods: ["GET", "PATCH"] },
  { path: new RegExp(`^projects/${SEGMENT}/move$`), methods: ["POST"] },
  { path: new RegExp(`^projects/${SEGMENT}/members$`), methods: ["GET"] },
  { path: new RegExp(`^projects/${SEGMENT}/members/${SEGMENT}$`), methods: ["PUT", "DELETE"] },
  { path: new RegExp(`^projects/${SEGMENT}/tasks$`), methods: ["GET", "POST"] },
  { path: new RegExp(`^projects/${SEGMENT}/statuses$`), methods: ["GET", "POST"] },
  { path: new RegExp(`^projects/${SEGMENT}/schedule$`), methods: ["GET"] },
  { path: new RegExp(`^projects/${SEGMENT}/dependencies$`), methods: ["GET", "POST"] },
  { path: new RegExp(`^tasks/${SEGMENT}$`), methods: ["GET", "PATCH", "DELETE"] },
  { path: new RegExp(`^tasks/${SEGMENT}/move$`), methods: ["POST"] },
  { path: new RegExp(`^statuses/${SEGMENT}$`), methods: ["PATCH", "DELETE"] },
  { path: new RegExp(`^dependencies/${SEGMENT}$`), methods: ["PATCH", "DELETE"] },
  { path: /^me\/tasks$/, methods: ["GET"] },
];

function apiUrl(): string {
  return (process.env.NEXUS_PROJECT_API_URL || "http://127.0.0.1:3152").replace(/\/+$/, "");
}

function webUrl(): string {
  return (process.env.NEXUS_PROJECT_WEB_URL || "http://127.0.0.1:8093").replace(/\/+$/, "");
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
  if (relativePath.startsWith("//")) return false;
  if (relativePath.includes("\\") || relativePath.includes("\0")) return false;

  for (const rawSegment of relativePath.split("/")) {
    if (!rawSegment) continue;
    let segment = rawSegment;
    try {
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

  return relativePath !== "/api" && !relativePath.startsWith("/api/");
}

function isHtml(upstream: Response): boolean {
  return upstream.headers.get("content-type")?.toLowerCase().includes("text/html") ?? false;
}

/**
 * Authenticated, fixed-surface proxy for Project's loopback API.
 */
export async function proxyProjectApi(req: Request, rest: string, search: string): Promise<Response> {
  const who = await callerIdentity(req);
  if (!who) return Response.json({ error: "not_authenticated" }, { status: 401 });

  const route = routeFor(rest);
  if (!route) return Response.json({ error: "not_found" }, { status: 404 });
  if (!route.methods.includes(req.method)) {
    return Response.json({ error: "method_not_allowed" }, { status: 405 });
  }

  const secret = process.env.NEXUS_PROJECT_DASHBOARD_SECRET;
  if (!secret) {
    console.error("[dashboard] NEXUS_PROJECT_DASHBOARD_SECRET is not set; project hop disabled");
    return unavailable();
  }

  const headers = safeRequestHeaders(req);
  headers.set("x-nexus-subject", who.subject);
  headers.set("x-nexus-dashboard-secret", secret);

  try {
    const upstream = new URL(`/api/v1/project/${rest}`, apiUrl());
    upstream.search = search;
    const response = await fetch(upstream, {
      method: req.method,
      headers,
      body: req.method === "GET" || req.method === "HEAD" ? undefined : await req.arrayBuffer(),
      signal: AbortSignal.timeout(API_TIMEOUT_MS),
    });
    if (response.status >= 500) {
      console.error(`[dashboard] project upstream ${response.status} for ${req.method} ${rest}`);
      return unavailable();
    }
    const responseHeaders = safeResponseHeaders(response);
    responseHeaders.set("cache-control", "no-store");
    responseHeaders.set("referrer-policy", "no-referrer");
    responseHeaders.set("x-content-type-options", "nosniff");
    return new Response(response.body, { status: response.status, headers: responseHeaders });
  } catch {
    return unavailable();
  }
}

/**
 * Serves Project's own web artifact below the Dashboard shell path.
 */
export async function proxyProjectWeb(req: Request, relativePath: string): Promise<Response> {
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
