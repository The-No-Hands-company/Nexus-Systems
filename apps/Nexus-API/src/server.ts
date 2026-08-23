/// <reference types="bun" />
import { randomUUID } from "node:crypto";
import { startNexusApiHeartbeat } from "./cloud";

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
    status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...h },
  });
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
    port,
    async fetch(request: Request) {
      const url = new URL(request.url);
      const path = url.pathname || "";
      const startTime = Date.now();

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-api", status: "ok", version: "v2", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) }, 200);
      }

      if (request.method === "GET" && path === "/api/v1/gateway/status") {
        const s: GatewayStats = { routesDefined: routes.size, routedTotal, errorsTotal, uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) };
        return json({ service: "nexus-api", ...s }, 200);
      }

      if (request.method === "GET" && path === "/api/v1/gateway/routes") {
        return json({ routes: Array.from(routes.values()) }, 200);
      }

      if (request.method === "POST" && path === "/api/v1/gateway/routes") {
        const body = await request.json().catch(() => ({})) as Record<string, unknown>;
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
        const [, rawMethod, rawPath] = routeKeyMatch;
        const key = `${rawMethod!.toUpperCase()}:${decodeURIComponent(rawPath!)}`;
        const deleted = routes.delete(key);
        return json({ deleted }, deleted ? 200 : 404);
      }

      // ── Proxy routing ──
      const requestKey = `${request.method}:${path}`;
      const matched = routes.get(requestKey);
      if (matched && matched.enabled) {
        const headers = new Headers(request.headers);
        headers.delete("host");
        headers.delete("content-length");
        if (authToken) headers.set("authorization", `Bearer ${authToken}`);

        const body = request.method === "GET" || request.method === "HEAD"
          ? undefined : await request.arrayBuffer();

        try {
          const upstream = await fetch(`${matched.target.replace(/\/$/, "")}${path}${url.search}`, {
            method: request.method, headers, body, redirect: "manual",
          });
          routedTotal++;
          const resHeaders = new Headers(upstream.headers);
          resHeaders.delete("content-length");
          return new Response(upstream.body, { status: upstream.status, headers: resHeaders });
        } catch (err) {
          errorsTotal++;
          return json({ error: "upstream unavailable", target: matched.target }, 502);
        }
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-api] Listening on port ${server.port}`);
  const stopHeartbeat = startNexusApiHeartbeat(baseUrl);

  return { server, close: () => { stopHeartbeat(); server.stop(); } };
}
