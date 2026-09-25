import { startHeartbeat } from "./cloud";
import { HttpError, errorResponse, json, notFound } from "./http";

export const API_PREFIX = "/api/v1/project";

export async function createServer() {
  const port = Number(process.env.PORT || "3152");
  const baseUrl = process.env.NEXUS_PROJECT_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();

  const server = Bun.serve({
    port,
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    async fetch(req) {
      const path = new URL(req.url).pathname;
      if (req.method === "GET" && path === "/health") {
        return json({
          service: "nexus-project",
          status: "ok",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
        });
      }
      if (req.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-project", status: "ready", capabilities: ["projects", "tasks", "scheduling"] });
      }
      return errorResponse(notFound());
    },
    error(error) {
      if (error instanceof HttpError) return errorResponse(error);
      console.error("[nexus-project] unhandled error:", error);
      return json({ error: "internal", message: "internal error" }, 500);
    },
  });

  console.log(`[nexus-project] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    close: () => {
      stopHeartbeat();
      server.stop(true);
    },
  };
}
