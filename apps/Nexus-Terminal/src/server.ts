import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { TerminalEngine } from "./terminal-engine";
import { spawnShell, getSession, sessionCount, reapIdle, killAll } from "./pty";
import { callerIdentity, terminalEnabled } from "./auth";
import { TerminalAudit } from "./audit";

/**
 * What travels with an upgraded socket.
 *
 * Declared so Bun.serve can be parameterised with it: without the type
 * argument `ws.data` is `undefined` and every use needs a cast, which is both
 * noisy and a place for a genuine mistake to hide.
 *
 * `sessionId` is filled in by open() once the shell exists — it cannot be
 * known at upgrade time.
 */
type ShellSocket = {
  subject: string;
  cols: number;
  rows: number;
  remoteIp: string | null;
  sessionId?: string;
};
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3110"); const baseUrl = process.env.NEXUS_NEXUS_TERMINAL_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new TerminalEngine("data/terminal.sqlite")
  const phantom = new PhantomApp("nexus-terminal");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const audit = new TerminalAudit();

  // Abandoned tabs leave live shells behind. Without reaping, the session
  // ceiling eventually refuses new ones and nobody can see why.
  const reaper = setInterval(() => reapIdle(), 60_000);

  const server = Bun.serve<ShellSocket>({ port,
    websocket: {
      open(ws) {
        const d = ws.data;
        const session = spawnShell({
          subject: d.subject,
          cols: d.cols,
          rows: d.rows,
          onData: (chunk) => { try { ws.send(chunk); } catch { /* socket gone */ } },
          onExit: (code) => {
            const id = ws.data.sessionId;
            if (id) audit.end(id, code);
            try { ws.close(); } catch { /* already closed */ }
          },
        });
        if (!session) { ws.close(1013, "too many sessions"); return; }
        ws.data.sessionId = session.id;
        audit.begin(session.id, d.subject, d.remoteIp);
      },
      message(ws, raw) {
        const id = ws.data.sessionId;
        if (!id) return;
        const session = getSession(id);
        if (!session) return;
        const text = typeof raw === "string" ? raw : new TextDecoder().decode(raw);
        // Recorded before it runs: an audit written afterwards loses exactly
        // the command that crashed the process.
        audit.input(id, text);
        session.write(text);
      },
      close(ws) {
        const id = ws.data.sessionId;
        if (id) getSession(id)?.kill();
      },
    },
    async fetch(request, srv) { const url = new URL(request.url); const path = url.pathname || "";

    // ── The shell ────────────────────────────────────────────────────────
    //
    // A WebSocket upgrade carrying a real pty on this host. See
    // docs/TERMINAL-SECURITY.md — the three checks below are the whole of what
    // stands between the internet and a shell here.
    if (path === "/api/v1/terminal/attach") {
      // Off unless deliberately switched on: deploying the code is not the
      // same act as deciding to hand out shells.
      if (!terminalEnabled()) return json({ error: "terminal_disabled" }, 403);

      // Identity from Auth, asked with the caller's own cookies — never from a
      // header or query parameter the browser chose to send.
      const who = await callerIdentity(request);
      if (!who) return json({ error: "not_authenticated" }, 401);

      if (sessionCount() >= 8) return json({ error: "too_many_sessions" }, 503);

      const cols = Number(url.searchParams.get("cols") || 80);
      const rows = Number(url.searchParams.get("rows") || 24);
      const remoteIp = request.headers.get("x-forwarded-for")?.split(",")[0]?.trim() ?? null;

      if (srv.upgrade(request, { data: { subject: who.subject, cols, rows, remoteIp } })) {
        return undefined as unknown as Response;
      }
      return json({ error: "upgrade_failed" }, 400);
    }

    if (request.method === "GET" && path === "/api/v1/terminal/audit") {
      const who = await callerIdentity(request);
      if (!who) return json({ error: "not_authenticated" }, 401);
      return json({ sessions: audit.recent() }, 200);
    }

    if (request.method === "GET" && path === "/health") {
        // phantom is reported here, not only on /api/v1/status. The scaffold's
        // own test has asserted its presence on /health since the app was
        // generated and has been failing ever since, because nothing ever ran
        // it. Health that omits a subsystem it depends on is also just less
        // useful.
        return json({ service: "nexus-terminal", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString(), phantom: phantom.status(), shellEnabled: terminalEnabled(), activeShells: sessionCount() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-terminal", status: "ready", capabilities: ["sessions","commands","terminal"], cloudIntegration: { enabled: (process.env["NEXUS_TERMINAL_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/terminal/sessions") { const uid = url.searchParams.get("userId") || undefined; return json(engine.listSessions(uid), 200); }
    if (request.method === "POST" && path === "/api/v1/terminal/sessions") { const b = await request.json().catch(() => ({})) as any; if (!b.userId) return json({ error: "userId required" }, 400); return json(engine.createSession(b.userId, b.type || "shell"), 201); }
    if (request.method === "POST" && path === "/api/v1/terminal/sessions/end") { const b = await request.json().catch(() => ({})) as any; if (!b.id) return json({ error: "id required" }, 400); engine.endSession(b.id); return json({ ok: true }, 200); }
    if (request.method === "GET" && path === "/api/v1/terminal/commands") { const sid = url.searchParams.get("sessionId"); if (!sid) return json({ error: "sessionId required" }, 400); return json(engine.getSessionHistory(sid), 200); }
    if (request.method === "POST" && path === "/api/v1/terminal/commands") { const b = await request.json().catch(() => ({})) as any; if (!b.sessionId || !b.command) return json({ error: "sessionId and command required" }, 400); return json(engine.logCommand(b.sessionId, b.command, b.output || "", b.exitCode || 0), 201); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-terminal] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl);
  if (!terminalEnabled()) console.log("[nexus-terminal] shell attach DISABLED (set NEXUS_TERMINAL_ENABLED=true)");
  return { server, engine, close: () => { clearInterval(reaper); killAll(); stopHeartbeat(); phantom.stop(); server.stop(); } }; }
