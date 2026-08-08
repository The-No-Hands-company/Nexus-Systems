// Nexus Systems — Production Reverse Proxy with Dynamic Routing
// Fetches routing configuration from Nexus-Cloud's /api/v1/routes endpoint
// and uses it to route subdomains to appropriate services.
// Falls back to static configuration if Nexus-Cloud is unavailable.

import { parse } from "node:path";

// Configuration
const PORT = Number(process.env.PROXY_PORT || "80");
const DOMAIN = process.env.DOMAIN || "nexussystems.vexr.dev";
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

// Route cache: { timestamp: number, routes: Record<string, string> }
let routeCache = { timestamp: 0, routes: {} };

// Fallback static configuration (used when Nexus-Cloud is unavailable)
const FALLBACK_CLOUD_UPSTREAM = process.env.CLOUD_UPSTREAM || "http://127.0.0.1:8787";
const FALLBACK_CHAT_UPSTREAM = process.env.CHAT_UPSTREAM || "http://127.0.0.1:3109";

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
      // Normalize the domain for comparison
      const domain = route.domain.toLowerCase().replace(/^www\./, "");
      map[domain] = `http://${route.upstream}`; // Ensure http:// prefix
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

// Extract subdomain from host (e.g., "cloud.example.com" -> "cloud")
function getSubdomain(host: string): string {
  if (!host || !DOMAIN) return "";
  const normalizedHost = host.toLowerCase().replace(/^www\./, "");
  if (normalizedHost === DOMAIN) return "";
  return normalizedHost.substring(0, (`.${DOMAIN}`.length) * -1);
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

async function handleRequest(req: Request): Promise<Response> {
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
      // Not our domain - return 404
      return new Response(`Host ${host} not served by this proxy`, { status: 404 });
    }

    // Get current routing configuration
    const routes = await getRoutes();
    
    // Find matching route
    let upstreamUrl: string | null = null;
    
    // Try exact match first (including www)
    const normalizedHost = host.replace(/^www\./, "");
    if (routes[normalizedHost]) {
      upstreamUrl = normalizeUpstream(routes[normalizedHost]);
    } 
    // Try without www prefix if not found
    else if (host.startsWith("www.") && routes[host.substring(4)]) {
      upstreamUrl = normalizeUpstream(routes[host.substring(4)]);
    }
    
    // If still not found, try to match by subdomain for wildcard-like behavior
    if (!upstreamUrl) {
      const subdomain = getSubdomain(host);
      if (subdomain && routes[subdomain]) {
        upstreamUrl = normalizeUpstream(routes[subdomain]);
      }
    }
    
    // Apply fallback logic if enabled and no route found
    if (!upstreamUrl && FALLBACK_ENABLED) {
      if (host === `cloud.${DOMAIN}` || host === `www.cloud.${DOMAIN}`) {
        upstreamUrl = FALLBACK_CLOUD_UPSTREAM;
      } else if (host === `chat.${DOMAIN}` || host === `www.chat.${DOMAIN}`) {
        upstreamUrl = FALLBACK_CHAT_UPSTREAM;
      }
      // Add more fallbacks as needed
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

      const proxied = new Request(upstream.toString(), {
        method: req.method,
        headers: req.headers,
        body: req.method !== "GET" && req.method !== "HEAD" ? await req.arrayBuffer() : undefined,
      });

      const resp = await fetch(proxied);
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

const server = Bun.serve({ port: PORT, fetch: handleRequest });
console.log(`[proxy] Listening on ${server.hostname}:${server.port}`);