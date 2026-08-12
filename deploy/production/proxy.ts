// Nexus Systems — Production Reverse Proxy with Dynamic Routing
// Fetches routing configuration from Nexus-Cloud's /api/v1/routes endpoint
// and uses it to route subdomains to appropriate services.
// Falls back to static configuration if Nexus-Cloud is unavailable.

import { gate } from "./gate";

// Configuration
const PORT = Number(process.env.PROXY_PORT || "80");
const DOMAIN = process.env.DOMAIN || "tnhc.dev";
const CLOUD_URL = process.env.CLOUD_URL || `http://127.0.0.1:8787`; // Nexus-Cloud URL
const POLL_INTERVAL_MS = Number(process.env.POLL_INTERVAL_MS || "30000"); // 30 seconds
const FALLBACK_ENABLED = process.env.FALLBACK_ENABLED !== "false"; // true by default
const CACHE_TTL_MS = Number(process.env.CACHE_TTL_MS || "60000"); // 1 minute

// Route structure matching Nexus-Cloud's /api/v1/routes response
interface NexusCloudRoute {
  domain: string;
  upstream: string;
  /**
   * Whether this host requires a signed-in user. Absent means public.
   *
   * Defaulting to public is deliberate: a route Cloud has not been taught
   * about must keep behaving exactly as it does today. Gating is opt-in, so a
   * missing or misspelled field can never lock an app's users out.
   */
  requiresAuth?: boolean;
}

/** Where a host resolves to, and whether reaching it needs a session. */
export interface RouteTarget {
  upstream: string;
  requiresAuth: boolean;
}

// Route cache: host (full domain, lowercased) -> target
let routeCache: { timestamp: number; routes: Record<string, RouteTarget> } = {
  timestamp: 0,
  routes: {},
};

// Fallback static configuration (used when Nexus-Cloud is unavailable)
const FALLBACK_CLOUD_UPSTREAM = process.env.CLOUD_UPSTREAM || "http://127.0.0.1:8787";
const FALLBACK_CHAT_UPSTREAM = process.env.CHAT_UPSTREAM || "http://127.0.0.1:3109";
// auth.<DOMAIN> is the sign-in host. Every app redirects an unauthenticated
// browser to https://auth.<DOMAIN>/login?redirect=..., so this is the one that
// has to resolve or single sign-on has nowhere to happen.
const FALLBACK_AUTH_UPSTREAM = process.env.AUTH_UPSTREAM || "http://127.0.0.1:4310";

// The default backend for the *.<DOMAIN> wildcard. Any on-domain host that is
// neither a registered app route nor one of the static app fallbacks above is
// treated as a deployed Nexus-Hosting site and handed to Hosting's site-proxy,
// which dispatches by host and serves its own 404 for a name it does not know.
// This is what makes every deployed site reachable through the single wildcard
// while apps keep precedence. Set to "" to restore the old behaviour of 404ing
// unmatched hosts (e.g. a node with no Hosting site-proxy running).
const HOSTING_SITE_UPSTREAM = process.env.HOSTING_SITE_UPSTREAM ?? "http://127.0.0.1:8090";

// Fetch latest routing configuration from Nexus-Cloud
async function fetchRouteConfig(): Promise<NexusCloudRoute[] | null> {
  try {
    const url = `${CLOUD_URL.replace(/\/+$/, "")}/api/v1/routes`;
    const response = await fetch(url, {
      headers: {
        Accept: "application/json",
        // Add auth if needed
        ...(process.env.NEXUS_CLOUD_API_KEY 
          ? { "X-Api-Key": process.env.NEXUS_CLOUD_API_KEY } 
          : {}),
      },
    });
    
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    
    const data = await response.json();
    return data.routes ?? [];
  } catch (err) {
    console.warn(`[proxy] Failed to fetch route config: ${err}`);
    return null;
  }
}

// Convert Nexus-Cloud routes to a simple host->upstream map
export function buildRouteMap(routes: NexusCloudRoute[]): Record<string, RouteTarget> {
  const map: Record<string, RouteTarget> = {};

  for (const route of routes) {
    if (route.domain && route.upstream) {
      // Cloud returns `domain` as a full hostname ("cloud.tnhc.dev") and
      // `upstream` as a full URL, protocol included. normalizeUpstream only
      // adds a scheme when one is genuinely missing — prefixing
      // unconditionally yields "http://http://host:port", whose URL hostname
      // parses as "http".
      const domain = route.domain.toLowerCase().replace(/^www\./, "");
      map[domain] = {
        upstream: normalizeUpstream(route.upstream),
        // Strict equality, not truthiness: only a real boolean true gates a
        // host, so a stray string or a typo cannot silently lock people out.
        requiresAuth: route.requiresAuth === true,
      };
    }
  }

  return map;
}

/** Seeds the route cache directly. Tests only — there is no live Cloud there. */
export function __setRoutesForTest(routes: Record<string, RouteTarget>): void {
  routeCache = { timestamp: Date.now(), routes };
}

// Get current routes (from cache or fresh fetch)
async function getRoutes(): Promise<Record<string, RouteTarget>> {
  const now = Date.now();
  
  // Return cached if still fresh
  if (now - routeCache.timestamp < CACHE_TTL_MS && Object.keys(routeCache.routes).length > 0) {
    return routeCache.routes;
  }
  
  // Try to fetch fresh config
  const freshRoutes = await fetchRouteConfig();
  if (freshRoutes !== null) {
    const routeMap = buildRouteMap(freshRoutes);
    routeCache = { timestamp: now, routes: routeMap };
    console.log(`[proxy] Updated route cache with ${Object.keys(routeMap).length} routes`);
    return routeMap;
  }
  
  // If fetch failed and we have cached data, use it (even if stale)
  if (Object.keys(routeCache.routes).length > 0) {
    console.log(`[proxy] Using stale cache (${Object.keys(routeCache.routes).length} routes)`);
    return routeCache.routes;
  }
  
  // No cache and no fetch - return empty (will use fallback in handler)
  return {};
}

// Simple wildcard matching: host ends with domain
function matchesDomain(host: string): boolean {
  if (!host || !DOMAIN) return false;
  const normalizedHost = host.toLowerCase().replace(/^www\./, "");
  return normalizedHost.endsWith(`.${DOMAIN}`) || normalizedHost === DOMAIN;
}

// Normalize upstream URL
function normalizeUpstream(upstream: string): string {
  if (!upstream) return "http://127.0.0.1:80"; // fallback to localhost
  
  // Ensure it has a protocol
  if (!/^https?:\/\//i.test(upstream)) {
    return `http://${upstream}`;
  }
  
  return upstream;
}

/** What a pending WebSocket upgrade needs to reach its upstream. */
export interface WsProxyData {
  upstreamUrl: string;
  identityToken: string | null;
  /** The address the browser asked for, so the upstream gets the same path. */
  requestUrl?: string;
}

/**
 * Browser socket -> upstream socket, for connections currently open.
 *
 * A WeakMap keyed by the server-side socket: when Bun drops the connection the
 * entry goes with it, so a long-running proxy cannot accumulate dead links.
 */
const wsLinks = new WeakMap<
  object,
  { upstream: WebSocket; pending: (string | Uint8Array)[]; isOpen: () => boolean }
>();

/**
 * Whether this request is asking to become a WebSocket.
 *
 * Both headers matter and both are case-insensitive in ways that bite:
 * `Connection` is a comma-separated list and may read "keep-alive, Upgrade",
 * so an equality check misses real browsers.
 */
export function isWebSocketUpgrade(req: Request): boolean {
  const upgrade = req.headers.get("upgrade");
  if (!upgrade || upgrade.toLowerCase() !== "websocket") return false;
  const connection = req.headers.get("connection") ?? "";
  return connection
    .toLowerCase()
    .split(",")
    .some((part) => part.trim() === "upgrade");
}

/**
 * Strip headers that must not survive a proxy hop.
 *
 * Hop-by-hop headers describe the previous connection, not the response, and
 * forwarding them misdescribes ours.
 *
 * `content-encoding` matters most and is the least obvious. Bun's fetch()
 * honours Content-Encoding transparently and hands back plain bytes, so by the
 * time we see the body it is already decoded — but the header still says gzip.
 * Passing it on tells the client to gunzip something that is not gzipped, and
 * it fails with ERR_CONTENT_DECODING_FAILED ("Decoding failed").
 *
 * This only ever broke real browsers. curl sends no Accept-Encoding unless
 * asked, so upstream returned plain bytes with no header and every check from
 * the command line passed; browsers always advertise gzip, so for them every
 * proxied response through this path was corrupt. An app could answer 200 to
 * everything measurable here and still be unusable in a browser.
 *
 * `content-length` goes for the same reason: it describes the encoded length.
 */
export function sanitizeResponseHeaders(headers: Headers): void {
  headers.delete("connection");
  headers.delete("keep-alive");
  headers.delete("proxy-authenticate");
  headers.delete("proxy-authorization");
  headers.delete("te");
  headers.delete("trailer");
  headers.delete("transfer-encoding");
  headers.delete("upgrade");
  headers.delete("content-encoding");
  headers.delete("content-length");
}

export async function handleRequest(
  req: Request,
  server?: { upgrade: (req: Request, opts: { data: WsProxyData }) => boolean },
): Promise<Response> {
  try {
    const url = new URL(req.url);
    const host = url.hostname.toLowerCase();
    
    // Handle CORS preflight
    if (req.method === "OPTIONS") {
      return new Response(null, {
        status: 204,
        headers: {
          "Access-Control-Allow-Origin": "*",
          "Access-Control-Allow-Methods": "GET, POST, PUT, DELETE, PATCH, OPTIONS",
          "Access-Control-Allow-Headers": "Content-Type, Authorization, X-Api-Key",
        },
      });
    }

    // Check if this host matches our domain
    if (!matchesDomain(host)) {
      // Liveness for the deployer, which polls http://localhost:8080/health.
      // Deliberately gated on the host *not* being ours: cloud.<DOMAIN>/health
      // and friends must keep reaching their upstreams, so this can only answer
      // a direct request to the proxy's own address, which otherwise 404s.
      if (url.pathname === "/health") {
        return Response.json({
          status: "ok",
          domain: DOMAIN,
          routes: Object.keys(routeCache.routes).length,
        });
      }
      // Not our domain - return 404
      return new Response(`Host ${host} not served by this proxy`, { status: 404 });
    }

    // Get current routing configuration
    const routes = await getRoutes();
    
    // Find matching route
    let upstreamUrl: string | null = null;
    // Only a route Cloud explicitly marked can gate. Every fallback below
    // leaves this false, so a host the route table does not know about stays
    // reachable exactly as it is today.
    let requiresAuth = false;

    // Cloud keys its routing table by full hostname, and buildRouteMap
    // lowercases and strips a leading "www." from both sides, so a single
    // exact lookup covers every route Cloud can hand us.
    const normalizedHost = host.replace(/^www\./, "");
    const matched = routes[normalizedHost];
    if (matched) {
      upstreamUrl = matched.upstream;
      requiresAuth = matched.requiresAuth;
    }

    // Apply fallback logic if enabled and no route found
    if (!upstreamUrl && FALLBACK_ENABLED) {
      if (host === `cloud.${DOMAIN}` || host === `www.cloud.${DOMAIN}`) {
        upstreamUrl = FALLBACK_CLOUD_UPSTREAM;
      } else if (host === `chat.${DOMAIN}` || host === `www.chat.${DOMAIN}`) {
        upstreamUrl = FALLBACK_CHAT_UPSTREAM;
      } else if (host === `auth.${DOMAIN}` || host === `www.auth.${DOMAIN}`) {
        upstreamUrl = FALLBACK_AUTH_UPSTREAM;
      } else if (host === DOMAIN || host === `www.${DOMAIN}`) {
        // Local convenience only. In production the apex belongs to the
        // marketing site on Cloudflare Pages and never reaches this proxy, so
        // nothing may depend on the apex resolving here — that assumption is
        // exactly what left sign-in served by the marketing SPA. Kept so a
        // local run without Pages in front still has something at the root.
        upstreamUrl = FALLBACK_AUTH_UPSTREAM;
      }
    }
    
    // Default backend for the wildcard: any remaining on-domain host is treated
    // as a deployed Nexus-Hosting site. We are past the matchesDomain() gate, so
    // host ends with .<DOMAIN>; every app route and the static app fallbacks have
    // already had their turn, so this cannot shadow them. Hosting's site-proxy
    // dispatches on x-forwarded-host (set in the proxy block below) and serves
    // its own 404 for a name it does not know, so forwarding here is safe.
    if (!upstreamUrl && HOSTING_SITE_UPSTREAM) {
      upstreamUrl = HOSTING_SITE_UPSTREAM;
    }

    // If we still don't have an upstream, return 404
    if (!upstreamUrl) {
      return new Response(`No route configured for host: ${host}`, { status: 404 });
    }

    // Login gate. Runs before anything is forwarded, so an app can never see a
    // request from someone the ecosystem has not authenticated.
    const decision = await gate(req, { upstream: upstreamUrl, requiresAuth });
    if (!decision.allow) return decision.response;

    // WebSockets cannot go through fetch(): it has no way to perform the
    // upgrade handshake, so every realtime connection to an app behind this
    // proxy simply failed. Hand these to Bun's own upgrade instead, and pump
    // frames between the two sockets. The gate has already run, so a gated
    // host still requires a session — and the identity token it minted rides
    // along to the upstream, which is the only way the app can tell who is on
    // the other end of a socket.
    if (isWebSocketUpgrade(req)) {
      if (!server) {
        return new Response("WebSocket upgrade unavailable", { status: 500 });
      }
      const ok = server.upgrade(req, {
        data: {
          upstreamUrl,
          identityToken: decision.identityToken,
          requestUrl: req.url,
        },
      });
      // Bun answers the handshake itself when upgrade() succeeds; returning a
      // Response here would fight it.
      return ok
        ? (undefined as unknown as Response)
        : new Response("Expected a WebSocket upgrade", { status: 400 });
    }

    // Proxy to upstream
    try {
      const upstream = new URL(req.url);
      const upstreamObj = new URL(upstreamUrl);
      upstream.hostname = upstreamObj.hostname;
      upstream.port = upstreamObj.port;

      // The body is re-read into a buffer here, so the inbound framing headers
      // no longer describe it. Forwarding the original content-length (and the
      // original host) alongside a freshly built body left upstreams reading a
      // truncated or empty payload — a form POST arrived with no fields, so the
      // handler behaved as though nothing had been submitted. Drop them and let
      // the runtime recompute.
      const forwardHeaders = new Headers(req.headers);
      forwardHeaders.delete("content-length");
      forwardHeaders.delete("host");
      forwardHeaders.delete("connection");

      // Identity is something only this proxy may assert. Strip any inbound
      // value first — without that, a client could set the header itself and
      // claim to be anyone on every public route — then attach the one the
      // gate minted, if it minted one.
      forwardHeaders.delete("x-nexus-identity");
      if (decision.identityToken) {
        forwardHeaders.set("x-nexus-identity", decision.identityToken);
      }
      forwardHeaders.delete("transfer-encoding");
      // Tell the upstream who it is actually answering as, which anything
      // generating absolute URLs or scoping a cookie needs.
      forwardHeaders.set("x-forwarded-host", host);
      forwardHeaders.set("x-forwarded-proto", url.protocol.replace(":", ""));

      const proxied = new Request(upstream.toString(), {
        method: req.method,
        headers: forwardHeaders,
        body: req.method !== "GET" && req.method !== "HEAD" ? await req.arrayBuffer() : undefined,
      });

      // redirect: "manual" is essential. fetch defaults to following redirects,
      // which means the proxy would chase a 3xx itself and hand the client the
      // final page instead of the redirect. That breaks anything that redirects:
      // the sign-in POST returns 303 with Set-Cookie, and following it here
      // swallowed both — the browser never received the session cookie and never
      // navigated, so a correct login looked like a failed one. A reverse proxy
      // must relay redirects, not resolve them.
      const resp = await fetch(proxied, { redirect: "manual" });
      const headers = new Headers(resp.headers);
      headers.set("Access-Control-Allow-Origin", "*");
      sanitizeResponseHeaders(headers);

      return new Response(resp.body, { status: resp.status, headers });
    } catch (err) {
      console.error(`[proxy] Proxy error for ${upstreamUrl}:`, err);
      return new Response(`Bad Gateway: Unable to reach upstream`, { status: 502 });
    }
  } catch (err) {
    console.error(`[proxy] Request handling error:`, err);
    return new Response(`Internal Server Error`, { status: 500 });
  }
}

// Startup logging
console.log(`[proxy] Starting on port ${PORT}`);
console.log(`[proxy] Domain: ${DOMAIN}`);
console.log(`[proxy] Fetching routing config from: ${CLOUD_URL}/api/v1/routes`);
console.log(`[proxy] Poll interval: ${POLL_INTERVAL_MS}ms`);
console.log(`[proxy] Fallback enabled: ${FALLBACK_ENABLED}`);
console.log(`[proxy] Cache TTL: ${CACHE_TTL_MS}ms`);

// Initialize cache on startup
getRoutes().then(() => {
  console.log(`[proxy] Initial route cache loaded`);
}).catch(err => {
  console.warn(`[proxy] Failed to load initial route cache: ${err}`);
});

// Refresh in the background so requests never pay the fetch latency and a
// newly registered subdomain starts resolving without waiting for a request to
// trip the TTL. A failed poll leaves the previous cache in place; getRoutes()
// still treats CACHE_TTL_MS as a synchronous backstop. Set POLL_INTERVAL_MS=0
// to rely on that lazy path alone.
/**
 * Starts the background route poller and begins listening.
 *
 * Both used to run at module scope, which made this file impossible to import:
 * doing so bound PORT — 8080 in production — and fought the running proxy for
 * it, while also starting a timer that polled Cloud forever. Everything with a
 * side effect now lives here, called only from the entrypoint below, so tests
 * and other callers can import handleRequest and the route helpers freely.
 */
export function startProxy() {
  // Refresh routes ahead of demand so a request rarely has to wait on Cloud to
  // trip the TTL. A failed poll leaves the previous cache in place; getRoutes()
  // still treats CACHE_TTL_MS as a synchronous backstop. Set POLL_INTERVAL_MS=0
  // to rely on that lazy path alone.
  if (POLL_INTERVAL_MS > 0) {
    setInterval(async () => {
      const fresh = await fetchRouteConfig();
      if (fresh !== null) {
        routeCache = { timestamp: Date.now(), routes: buildRouteMap(fresh) };
      }
    }, POLL_INTERVAL_MS);
  }

  // The one service that binds every interface on purpose. cloudflared runs in
  // a bridge-network container and reaches this over the host's LAN address, so
  // loopback would cut the tunnel off entirely. Every service behind here binds
  // 127.0.0.1 instead, which is what makes this the single way in — and so the
  // single place the login gate has to live.
  const server = Bun.serve<WsProxyData>({
    port: PORT,
    hostname: process.env.NEXUS_PROXY_BIND_HOST || "0.0.0.0",
    fetch: (req, srv) => handleRequest(req, srv),

    // ── WebSocket relay ────────────────────────────────────────────────────
    // One socket in from the browser, one socket out to the app, and frames
    // copied between them. Nothing here inspects payloads: this is a pipe, and
    // the app on the far end is what decides meaning.
    websocket: {
      // Realtime chat frames are small; this bounds a client that decides to
      // send something enormous.
      maxPayloadLength: 16 * 1024 * 1024,
      idleTimeout: 300,

      open(ws) {
        const { upstreamUrl, identityToken } = ws.data;
        const target = new URL(upstreamUrl);
        const asked = new URL(ws.data.requestUrl ?? "http://localhost/");
        target.protocol = target.protocol === "https:" ? "wss:" : "ws:";
        target.pathname = asked.pathname;
        target.search = asked.search;

        // Frames can arrive from the browser before the upstream finishes
        // connecting. Dropping them would silently lose whatever the client
        // said first — for this gateway that is the Identify message, so the
        // session would hang forever waiting for a READY that never comes.
        const pending: (string | Uint8Array)[] = [];
        let upstreamOpen = false;

        const headers: Record<string, string> = {};
        if (identityToken) headers["x-nexus-identity"] = identityToken;

        const upstream = new WebSocket(target.toString(), { headers } as never);
        upstream.binaryType = "arraybuffer";

        upstream.onopen = () => {
          upstreamOpen = true;
          for (const frame of pending.splice(0)) upstream.send(frame);
        };
        upstream.onmessage = (event: MessageEvent) => {
          ws.send(
            typeof event.data === "string"
              ? event.data
              : new Uint8Array(event.data as ArrayBuffer),
          );
        };
        // 1011 rather than a normal close: the client did nothing wrong, and a
        // clean 1000 would tell it not to retry.
        upstream.onclose = (event: CloseEvent) => {
          try {
            ws.close(event.code >= 1000 && event.code <= 4999 ? event.code : 1011, event.reason);
          } catch { /* already gone */ }
        };
        upstream.onerror = () => {
          try { ws.close(1011, "upstream error"); } catch { /* already gone */ }
        };

        wsLinks.set(ws, { upstream, pending, isOpen: () => upstreamOpen });
      },

      message(ws, message) {
        const link = wsLinks.get(ws);
        if (!link) return;
        const frame = typeof message === "string" ? message : new Uint8Array(message);
        if (link.isOpen()) link.upstream.send(frame);
        else link.pending.push(frame);
      },

      close(ws, code, reason) {
        const link = wsLinks.get(ws);
        wsLinks.delete(ws);
        if (!link) return;
        try {
          link.upstream.close(code >= 1000 && code <= 4999 ? code : 1000, reason);
        } catch { /* already gone */ }
      },
    },
  });
  console.log(`[proxy] Listening on ${server.hostname}:${server.port}`);
  return server;
}

// Only when executed directly (`bun run proxy.ts`), never on import.
if (import.meta.main) {
  startProxy();
}