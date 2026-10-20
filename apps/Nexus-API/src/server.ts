import { buildApiContract } from "./api-contract";
import { startNexusApiHeartbeat } from "./cloud";
import { applyApiCors, preflightApiResponse } from "./cors";
import { buildOpenApiDocument } from "./openapi";

interface Route {
  path: string;
  method: string;
  target: string;
  enabled: boolean;
}

interface GatewayStats {
  routesDefined: number;
  routedTotal: number;
  errorsTotal: number;
  uptimeSeconds: number;
}

function json(payload: unknown, s: number, h?: Record<string, string>): Response {
  return new Response(JSON.stringify(payload), {
    status: s,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "x-request-id": crypto.randomUUID(),
      ...h,
    },
  });
}

function serviceUrl(name: "auth" | "cloud" | "mail" | "calendar"): string {
  const fallbacks: Record<string, string> = {
    auth: "http://127.0.0.1:4310",
    cloud: "http://127.0.0.1:8787",
    mail: "http://127.0.0.1:3140",
    calendar: "http://127.0.0.1:3068",
  };
  const envNames: Record<string, string> = {
    auth: "NEXUS_API_AUTH_URL",
    cloud: "NEXUS_API_CLOUD_URL",
    mail: "NEXUS_API_MAIL_URL",
    calendar: "NEXUS_API_CALENDAR_URL",
  };
  return (process.env[envNames[name]] || fallbacks[name]).trim().replace(/\/+$/, "");
}

function credentialHeaders(request: Request): Headers {
  const headers = new Headers({ accept: "application/json" });
  const cookie = request.headers.get("cookie");
  const authorization = request.headers.get("authorization");
  if (cookie) headers.set("cookie", cookie);
  if (authorization) headers.set("authorization", authorization);
  return headers;
}

async function callerFromAuth(
  request: Request,
): Promise<Response | { user: Record<string, unknown> }> {
  try {
    const upstream = await fetch(`${serviceUrl("auth")}/api/v1/auth/me`, {
      headers: credentialHeaders(request),
      signal: AbortSignal.timeout(3000),
    });
    const body = await upstream.text();
    if (!upstream.ok) {
      return json({ error: "not_authenticated" }, 401, {
        "cache-control": "no-store",
      });
    }
    let parsed: unknown;
    try {
      parsed = JSON.parse(body);
    } catch {
      parsed = null;
    }
    const user =
      parsed && typeof parsed === "object" && "user" in parsed
        ? (parsed as { user?: unknown }).user
        : null;
    if (!user || typeof user !== "object") {
      return json({ error: "identity_unavailable" }, 503);
    }
    return { user: user as Record<string, unknown> };
  } catch {
    return json({ error: "identity_unavailable" }, 503);
  }
}

function operatorAuthorized(request: Request): boolean {
  const expected = (
    process.env.NEXUS_API_OPERATOR_TOKEN ||
    process.env.NEXUS_API_AUTH_TOKEN ||
    process.env.NEXUS_CLOUD_API_KEY ||
    ""
  ).trim();
  // Local tests and development can exercise the compatibility gateway without
  // a secret. Production must configure one and then fails closed.
  if (!expected) return process.env.NODE_ENV !== "production";
  const bearer = request.headers.get("authorization")?.match(/^Bearer\s+(.+)$/i)?.[1];
  const apiKey = request.headers.get("x-api-key");
  return bearer === expected || apiKey === expected;
}

async function forwardCatalog(request: Request): Promise<Response> {
  // The catalog is intentionally public: the signed-out home page needs it to
  // show "Running right now". We attempt auth so the caller's identity is
  // available for future personalisation, but we never gate the catalog on it.
  await callerFromAuth(request);

  const headers = new Headers({ accept: "application/json" });
  const apiKey = process.env.NEXUS_API_CLOUD_API_KEY || process.env.NEXUS_CLOUD_API_KEY;
  if (apiKey) headers.set("x-api-key", apiKey);

  try {
    let upstream = await fetch(`${serviceUrl("cloud")}/api/v1/catalog`, {
      headers,
      signal: AbortSignal.timeout(5000),
    });
    if (upstream.status === 404) {
      upstream = await fetch(`${serviceUrl("cloud")}/api/v1/tools`, {
        headers,
        signal: AbortSignal.timeout(5000),
      });
    }
    const body = upstream.ok ? releasedCatalogBody(await upstream.text()) : await upstream.text();
    return new Response(body, {
      status: upstream.ok ? 200 : upstream.status >= 500 ? 503 : upstream.status,
      headers: {
        "content-type": upstream.headers.get("content-type") ?? "application/json",
        "cache-control": "private, no-store",
      },
    });
  } catch {
    return json({ error: "catalog_unavailable" }, 503);
  }
}

function callerSubject(caller: { user: Record<string, unknown> }): string | null {
  const id = caller.user.id;
  return typeof id === "string" && id ? id : null;
}

/**
 * The platform facade exposes only the user-facing release lane. Cloud may
 * return staged, retired, hidden, or admin-only records to trusted control
 * plane callers; those records must never become a public launcher contract.
 */
function releasedCatalogBody(body: string): string {
  try {
    const parsed = JSON.parse(body) as { apps?: unknown; tools?: unknown };
    const key = Array.isArray(parsed.apps) ? "apps" : Array.isArray(parsed.tools) ? "tools" : null;
    if (!key) return JSON.stringify({ apps: [] });
    const entries = parsed[key] as unknown[];
    const apps = entries.filter((entry) => {
      if (!entry || typeof entry !== "object") return false;
      const item = entry as Record<string, unknown>;
      return (
        item.surface === "user-app" &&
        item.releaseState === "released" &&
        item.maturity === "stable" &&
        item.reviewStatus === "approved" &&
        item.visibility !== "hidden"
      );
    });
    return JSON.stringify({ [key]: apps });
  } catch {
    return JSON.stringify({ apps: [] });
  }
}

async function forwardMail(request: Request, rest: string, search: string): Promise<Response> {
  const caller = await callerFromAuth(request);
  if (caller instanceof Response) return caller;
  const subject = callerSubject(caller);
  if (!subject) return json({ error: "identity_unavailable" }, 503);

  const allowed =
    /^(folders(?:\/[^/]+\/messages)?|messages(?:\/[^/]+(?:\/seen|\/attachments\/[^/]+)?)?|search|threads\/[^/]+\/messages)$/;
  if (!allowed.test(rest)) return json({ error: "not_found" }, 404);
  if (request.method !== "GET" && request.method !== "POST") {
    return json({ error: "method_not_allowed" }, 405);
  }

  const headers = new Headers({
    accept: request.headers.get("accept") ?? "application/json",
    "x-nexus-subject": subject,
  });
  const contentType = request.headers.get("content-type");
  if (contentType) headers.set("content-type", contentType);
  try {
    const upstream = await fetch(`${serviceUrl("mail")}/api/v1/${rest}${search}`, {
      method: request.method,
      headers,
      body: request.method === "POST" ? await request.arrayBuffer() : undefined,
      signal: AbortSignal.timeout(10000),
    });
    const responseHeaders = new Headers();
    for (const name of ["content-type", "content-disposition", "x-content-type-options"]) {
      const value = upstream.headers.get(name);
      if (value) responseHeaders.set(name, value);
    }
    if (!responseHeaders.has("content-type"))
      responseHeaders.set("content-type", "application/json");
    return new Response(upstream.body, { status: upstream.status, headers: responseHeaders });
  } catch {
    return json({ error: "mail_unavailable" }, 503);
  }
}

const PUBLIC_TOKEN_RE = /^[A-Za-z0-9_-]{43}$/;

async function forwardCalendar(request: Request, rest: string, search: string): Promise<Response> {
  // Public share: no session required — the token IS the access credential.
  if (rest.startsWith("public/")) {
    if (request.method !== "GET") return json({ error: "method_not_allowed" }, 405);
    const token = rest.slice("public/".length);
    if (!PUBLIC_TOKEN_RE.test(token)) return json({ error: "not_found" }, 404);
    try {
      const upstream = await fetch(`${serviceUrl("calendar")}/api/v1/calendar/${rest}`, {
        headers: { accept: "application/json" },
        signal: AbortSignal.timeout(5000),
      });
      const ct = upstream.headers.get("content-type");
      return new Response(upstream.body, {
        status: upstream.status,
        headers: { "content-type": ct ?? "application/json", "cache-control": "no-store" },
      });
    } catch {
      return json({ error: "calendar_unavailable" }, 503);
    }
  }

  // Session routes — allowlist: events (list/create) and events/:id (read/update/delete).
  // Sharing mutation routes (/shares, /public-share) are kept owner-private.
  const allowed = /^events(?:\/[^/]+)?$/;
  if (!allowed.test(rest)) return json({ error: "not_found" }, 404);
  if (
    request.method !== "GET" &&
    request.method !== "POST" &&
    request.method !== "PATCH" &&
    request.method !== "DELETE"
  ) {
    return json({ error: "method_not_allowed" }, 405);
  }

  const caller = await callerFromAuth(request);
  if (caller instanceof Response) return caller;
  const subject = callerSubject(caller);
  if (!subject) return json({ error: "identity_unavailable" }, 503);

  const secret = (process.env.NEXUS_CALENDAR_DASHBOARD_SECRET || "").trim();
  if (!secret) return json({ error: "calendar_unavailable" }, 503);

  const headers = new Headers({
    accept: request.headers.get("accept") ?? "application/json",
    "x-nexus-subject": subject,
    "x-nexus-dashboard-secret": secret,
  });
  const contentType = request.headers.get("content-type");
  if (contentType) headers.set("content-type", contentType);

  try {
    const upstream = await fetch(`${serviceUrl("calendar")}/api/v1/calendar/${rest}${search}`, {
      method: request.method,
      headers,
      body:
        request.method === "POST" || request.method === "PATCH"
          ? await request.arrayBuffer()
          : undefined,
      signal: AbortSignal.timeout(10000),
    });
    const responseHeaders = new Headers();
    const ct = upstream.headers.get("content-type");
    responseHeaders.set("content-type", ct ?? "application/json");
    return new Response(upstream.body, { status: upstream.status, headers: responseHeaders });
  } catch {
    return json({ error: "calendar_unavailable" }, 503);
  }
}

export function createApiServer() {
  const port = Number(process.env.PORT || "3036");
  const baseUrl = process.env.NEXUS_API_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const routes = new Map<string, Route>();
  let routedTotal = 0;
  let errorsTotal = 0;

  const authToken = process.env.NEXUS_API_AUTH_TOKEN || "";

  const server = Bun.serve({
    hostname: process.env.NEXUS_API_BIND_HOST || "127.0.0.1",
    port,
    async fetch(request: Request) {
      const preflight = preflightApiResponse(request);
      if (preflight) return preflight;

      const respond = async (): Promise<Response> => {
        const url = new URL(request.url);
        const path = url.pathname || "";
        if (request.method === "GET" && path === "/health") {
          return json(
            {
              service: "nexus-api",
              status: "ok",
              version: process.env.NEXUS_API_VERSION?.trim() || "1.0.0",
              uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
            },
            200,
          );
        }

        if (request.method === "GET" && path === "/api/v1/contract") {
          return json(buildApiContract(baseUrl), 200);
        }

        if (request.method === "GET" && path === "/api/v1/openapi.json") {
          return json(buildOpenApiDocument(buildApiContract(baseUrl)), 200, {
            "cache-control": "public, max-age=300",
          });
        }

        if (request.method === "GET" && path === "/api/v1/me") {
          const caller = await callerFromAuth(request);
          return caller instanceof Response
            ? caller
            : json({ user: caller.user }, 200, { "cache-control": "private, no-store" });
        }

        if (request.method === "GET" && (path === "/api/v1/apps" || path === "/api/v1/catalog")) {
          return forwardCatalog(request);
        }

        if (path === "/api/v1/calendar" || path.startsWith("/api/v1/calendar/")) {
          return forwardCalendar(request, path.replace(/^\/api\/v1\/calendar\/?/, ""), url.search);
        }

        if (path === "/api/v1/mail" || path.startsWith("/api/v1/mail/")) {
          return forwardMail(request, path.replace(/^\/api\/v1\/mail\/?/, ""), url.search);
        }

        if (request.method === "GET" && path === "/api/v1/gateway/status") {
          const s: GatewayStats = {
            routesDefined: routes.size,
            routedTotal,
            errorsTotal,
            uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          };
          return json({ service: "nexus-api", ...s }, 200);
        }

        if (request.method === "GET" && path === "/api/v1/gateway/routes") {
          if (!operatorAuthorized(request)) return json({ error: "operator_auth_required" }, 401);
          return json({ routes: Array.from(routes.values()) }, 200);
        }

        if (request.method === "POST" && path === "/api/v1/gateway/routes") {
          if (!operatorAuthorized(request)) return json({ error: "operator_auth_required" }, 401);
          const body = (await request.json().catch(() => ({}))) as Record<string, unknown>;
          const routePath = typeof body.path === "string" ? body.path.trim() : "";
          const method = typeof body.method === "string" ? body.method.toUpperCase() : "GET";
          const target = typeof body.target === "string" ? body.target.trim() : "";
          if (!routePath || !target) return json({ error: "path and target are required" }, 400);

          const key = `${method}:${routePath}`;
          routes.set(key, { path: routePath, method, target, enabled: true });
          return json({ status: "ok", route: routes.get(key) }, 201);
        }

        const routeKeyMatch = path.match(/^\/api\/v1\/gateway\/routes\/([^/]+)\/([^/]+)$/);
        if (request.method === "DELETE" && routeKeyMatch) {
          if (!operatorAuthorized(request)) return json({ error: "operator_auth_required" }, 401);
          const rawMethod = routeKeyMatch[1];
          const rawPath = routeKeyMatch[2];
          if (!rawMethod || !rawPath) return json({ error: "invalid route" }, 400);
          const key = `${rawMethod.toUpperCase()}:${decodeURIComponent(rawPath)}`;
          const deleted = routes.delete(key);
          return json({ deleted }, deleted ? 200 : 404);
        }

        // ── Proxy routing ──
        const requestKey = `${request.method}:${path}`;
        const matched = routes.get(requestKey);
        if (matched?.enabled) {
          const headers = new Headers(request.headers);
          headers.delete("host");
          headers.delete("content-length");
          if (authToken) headers.set("authorization", `Bearer ${authToken}`);

          const body =
            request.method === "GET" || request.method === "HEAD"
              ? undefined
              : await request.arrayBuffer();

          try {
            const upstream = await fetch(
              `${matched.target.replace(/\/$/, "")}${path}${url.search}`,
              {
                method: request.method,
                headers,
                body,
                redirect: "manual",
              },
            );
            routedTotal++;
            const resHeaders = new Headers(upstream.headers);
            resHeaders.delete("content-length");
            return new Response(upstream.body, { status: upstream.status, headers: resHeaders });
          } catch {
            errorsTotal++;
            return json({ error: "upstream unavailable", target: matched.target }, 502);
          }
        }

        return json({ error: "not found" }, 404);
      }; // end respond
      return applyApiCors(request, await respond());
    },
  });

  console.log(`[nexus-api] Listening on port ${server.port}`);
  const stopHeartbeat = startNexusApiHeartbeat(baseUrl);

  return {
    server,
    close: () => {
      stopHeartbeat();
      server.stop();
    },
  };
}
