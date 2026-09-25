import type { Database } from "bun:sqlite";
import { mkdirSync } from "node:fs";
import { dirname } from "node:path";
import { resolveCaller } from "./auth";
import { startHeartbeat } from "./cloud";
import { HttpError, errorResponse, json, notFound } from "./http";
import { Router } from "./router";
import { openDatabase } from "./store/db";
import { registerProjectRoutes } from "./routes/projects";
import { registerWorkspaceRoutes } from "./routes/workspaces";
import { registerStatusRoutes } from "./routes/statuses";

export const API_PREFIX = "/api/v1/project";

export interface Context {
  req: Request;
  url: URL;
  db: Database;
  subject: string;
}

/** Each later task appends its register function here. */
const ROUTE_MODULES: ((router: Router<Context>) => void)[] = [registerWorkspaceRoutes, registerProjectRoutes, registerStatusRoutes];

export async function createServer() {
  const port = Number(process.env.PORT || "3152");
  const baseUrl = process.env.NEXUS_PROJECT_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const dbPath = process.env.NEXUS_PROJECT_DB || "data/project.sqlite";
  if (dbPath !== ":memory:") mkdirSync(dirname(dbPath), { recursive: true });
  const db = openDatabase(dbPath);

  const router = new Router<Context>();
  for (const register of ROUTE_MODULES) register(router);

  const server = Bun.serve({
    port,
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    async fetch(req) {
      const url = new URL(req.url);
      const path = url.pathname;

      // Public: liveness and capabilities only. Neither reads project data.
      if (req.method === "GET" && path === "/health") {
        return json({ service: "nexus-project", status: "ok", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      }
      if (req.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-project", status: "ready", capabilities: ["projects", "tasks", "scheduling"] });
      }
      if (path !== API_PREFIX && !path.startsWith(`${API_PREFIX}/`)) return errorResponse(notFound());

      // Identity before routing: an anonymous caller learns nothing, not even
      // which paths exist.
      const caller = await resolveCaller(req);
      if (!caller) return errorResponse(new HttpError(401, "not_authenticated", "sign in to use Nexus Project"));

      try {
        const match = router.match(req.method, path.slice(API_PREFIX.length));
        if (!match) throw notFound();
        if ("allowed" in match) {
          return json({ error: "method_not_allowed", message: `use ${match.allowed.join(", ")}` }, 405, {
            allow: match.allowed.join(", "),
          });
        }
        return await match.handler({ req, url, db, subject: caller.subject }, match.params);
      } catch (error) {
        if (error instanceof HttpError) return errorResponse(error);
        // Internal detail goes to the operator's log, never to the client.
        console.error(`[nexus-project] ${req.method} ${path} failed:`, error);
        return json({ error: "internal", message: "internal error" }, 500);
      }
    },
  });

  console.log(`[nexus-project] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    db,
    close: () => {
      stopHeartbeat();
      server.stop(true);
      db.close();
    },
  };
}
