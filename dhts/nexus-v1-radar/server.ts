import { serve } from "bun";
import { resolve, join } from "path";
import { existsSync, readFileSync } from "fs";
import { auditEcosystem, auditApp, type EcosystemAuditSummary } from "./scan";
import { REGISTERED_APPS } from "./criteria";

const PORT = parseInt(process.env.PORT || "3200", 10);
const PUBLIC_DIR = resolve(import.meta.dir, "public");
const CACHE_TTL_MS = 60_000; // refresh every 60s in the background

let cachedSummary: EcosystemAuditSummary | null = null;
let lastScannedTime = 0;
let scanInProgress = false;

function runBackgroundScan() {
  if (scanInProgress) return;
  scanInProgress = true;
  auditEcosystem()
    .then((s) => { cachedSummary = s; lastScannedTime = Date.now(); })
    .catch((err) => console.error("[radar] background scan error:", err?.message ?? err))
    .finally(() => { scanInProgress = false; });
}

function getSummary(force = false): EcosystemAuditSummary | null {
  const now = Date.now();
  if (force || !cachedSummary || now - lastScannedTime > CACHE_TTL_MS) {
    runBackgroundScan();
  }
  return cachedSummary;
}

// Kick off initial scan at startup
runBackgroundScan();

const server = serve({
  port: PORT,
  async fetch(req) {
    const url = new URL(req.url);
    const path = url.pathname;

    const corsHeaders = {
      "access-control-allow-origin": "*",
      "access-control-allow-methods": "GET, POST, OPTIONS",
      "access-control-allow-headers": "Content-Type",
    };

    if (req.method === "OPTIONS") {
      return new Response(null, { status: 204, headers: corsHeaders });
    }

    // API endpoints
    if (path === "/api/summary") {
      const summary = getSummary(false);
      if (!summary) return Response.json({ scanning: true }, { status: 202, headers: corsHeaders });
      return Response.json(summary, { headers: corsHeaders });
    }

    if (path === "/api/rescan") {
      const summary = getSummary(true);
      if (!summary) return Response.json({ scanning: true }, { status: 202, headers: corsHeaders });
      return Response.json(summary, { headers: corsHeaders });
    }

    if (path.startsWith("/api/app/")) {
      const appId = path.replace("/api/app/", "");
      const app = REGISTERED_APPS.find((a) => a.id === appId);
      if (!app) return Response.json({ error: "app_not_found" }, { status: 404, headers: corsHeaders });
      try {
        const result = await auditApp(app);
        return Response.json(result, { headers: corsHeaders });
      } catch (err: any) {
        console.error("[radar] /api/app error:", err?.message ?? err);
        return Response.json({ error: "scan_failed", detail: String(err?.message ?? err) }, { status: 500, headers: corsHeaders });
      }
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
        headers: { "Content-Type": contentType, ...corsHeaders },
      });
    }

    return new Response("Not found", { status: 404, headers: corsHeaders });
  },
});

console.log(`\n\x1b[32m✔ Nexus Ground-Truth Radar Web Dashboard running at http://localhost:${server.port}\x1b[0m`);
console.log(`\x1b[90mProbing ecosystem daemons & public endpoints in real time. Press Ctrl+C to stop.\x1b[0m\n`);
