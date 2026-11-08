import { serve } from "bun";
import { resolve, join } from "path";
import { existsSync, readFileSync } from "fs";
import { auditEcosystem, auditApp } from "./scan";
import { REGISTERED_APPS } from "./criteria";

const PORT = parseInt(process.env.PORT || "3200", 10);
const PUBLIC_DIR = resolve(import.meta.dir, "public");

const server = serve({
  port: PORT,
  async fetch(req) {
    const url = new URL(req.url);
    const path = url.pathname;

    // API endpoints
    if (path === "/api/summary") {
      const summary = await auditEcosystem();
      return Response.json(summary);
    }

    if (path.startsWith("/api/app/")) {
      const appId = path.replace("/api/app/", "");
      const app = REGISTERED_APPS.find((a) => a.id === appId);
      if (!app) return Response.json({ error: "app_not_found" }, { status: 404 });
      const result = await auditApp(app);
      return Response.json(result);
    }

    // Static files
    let filePath = join(PUBLIC_DIR, path === "/" ? "index.html" : path);
    if (!existsSync(filePath)) {
      filePath = join(PUBLIC_DIR, "index.html");
    }

    if (existsSync(filePath)) {
      const ext = filePath.split(".").pop();
      const contentTypes: Record<string, string> = {
        html: "text/html; charset=utf-8",
        css: "text/css; charset=utf-8",
        js: "application/javascript; charset=utf-8",
        json: "application/json; charset=utf-8",
        svg: "image/svg+xml",
      };
      const contentType = contentTypes[ext || "html"] || "text/plain";
      return new Response(readFileSync(filePath), {
        headers: { "Content-Type": contentType },
      });
    }

    return new Response("Not found", { status: 404 });
  },
});

console.log(`\n\x1b[32m✔ Nexus v1.0 Radar Web Dashboard running at http://localhost:${server.port}\x1b[0m`);
console.log(`\x1b[90mPress Ctrl+C to stop.\x1b[0m\n`);
