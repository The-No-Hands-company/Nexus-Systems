/**
 * Loopback address of the stable platform facade. Production sets this to
 * Nexus-API; local development can leave it empty and use the legacy direct
 * service paths until the facade is running.
 */
export function platformApiUrl(): string {
  return (process.env.NEXUS_API_INTERNAL_URL || "").trim().replace(/\/+$/, "");
}

/** Forward only credentials that the platform facade understands. */
export function platformApiHeaders(request: Request): Headers {
  const headers = new Headers({ accept: "application/json" });
  const cookie = request.headers.get("cookie");
  const authorization = request.headers.get("authorization");
  if (cookie) headers.set("cookie", cookie);
  if (authorization) headers.set("authorization", authorization);
  return headers;
}
