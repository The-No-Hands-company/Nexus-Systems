const METHODS = "GET, POST, DELETE, PATCH, PUT, OPTIONS";
const HEADERS = "content-type, authorization, x-api-key";

export function allowedApiOrigins(): Set<string> {
  const configured = (process.env.NEXUS_API_ALLOWED_ORIGINS || "https://app.tnhc.dev")
    .split(",")
    .map((o) => o.trim().replace(/\/+$/, ""))
    .filter(Boolean);
  return new Set(configured);
}

export function preflightApiResponse(request: Request): Response | null {
  if (request.method !== "OPTIONS") return null;
  const origin = request.headers.get("origin") ?? "";
  if (!origin) return null;
  if (!allowedApiOrigins().has(origin)) {
    return new Response(null, { status: 403 });
  }
  return new Response(null, {
    status: 204,
    headers: {
      "access-control-allow-origin": origin,
      "access-control-allow-credentials": "true",
      "access-control-allow-methods": METHODS,
      "access-control-allow-headers": HEADERS,
      "access-control-max-age": "86400",
      vary: "Origin",
    },
  });
}

export function applyApiCors(request: Request, response: Response): Response {
  const origin = request.headers.get("origin") ?? "";
  if (!origin || !allowedApiOrigins().has(origin)) return response;
  const headers = new Headers(response.headers);
  headers.set("access-control-allow-origin", origin);
  headers.set("access-control-allow-credentials", "true");
  headers.set("vary", "Origin");
  return new Response(response.body, { status: response.status, headers });
}
