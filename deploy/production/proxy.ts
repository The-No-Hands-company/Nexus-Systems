// Nexus Systems — Production Reverse Proxy with Dynamic Routing
// Fetches routing configuration from Nexus-Cloud's /api/v1/routes endpoint
// and uses it to route subdomains to appropriate services.
// Falls back to static configuration if Nexus-Cloud is unavailable.

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
  // We don't need the other fields for routing
}

// Route cache: host (full domain, lowercased) -> upstream URL
let routeCache: { timestamp: number; routes: Record<string, string> } = {
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
function buildRouteMap(routes: NexusCloudRoute[]): Record<string, string> {
  const map: Record<string, string> = {};
  
  for (const route of routes) {
    if (route.domain && route.upstream) {
      // Cloud returns `domain` as a full hostname ("cloud.tnhc.dev") and
      // `upstream` as a full URL, protocol included. normalizeUpstream only
      // adds a scheme when one is genuinely missing — prefixing
      // unconditionally yields "http://http://host:port", whose URL hostname
      // parses as "http".
      const domain = route.domain.toLowerCase().replace(/^www\./, "");
      map[domain] = normalizeUpstream(route.upstream);
    }
  }

  return map;
}

// Get current routes (from cache or fresh fetch)
async function getRoutes(): Promise<Record<string, string>> {
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

export async function handleRequest(req: Request): Promise<Response> {
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
    
    // Cloud keys its routing table by full hostname, and buildRouteMap
    // lowercases and strips a leading "www." from both sides, so a single
    // exact lookup covers every route Cloud can hand us.
    const normalizedHost = host.replace(/^www\./, "");
    if (routes[normalizedHost]) {
      upstreamUrl = routes[normalizedHost];
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
      // Remove hop-by-hop headers that shouldn't be forwarded
      headers.delete("connection");
      headers.delete("keep-alive");
      headers.delete("proxy-authenticate");
      headers.delete("proxy-authorization");
      headers.delete("te");
      headers.delete("trailer");
      headers.delete("transfer-encoding");
      headers.delete("upgrade");
      
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

  const server = Bun.serve({ port: PORT, fetch: handleRequest });
  console.log(`[proxy] Listening on ${server.hostname}:${server.port}`);
  return server;
}

// Only when executed directly (`bun run proxy.ts`), never on import.
if (import.meta.main) {
  startProxy();
}