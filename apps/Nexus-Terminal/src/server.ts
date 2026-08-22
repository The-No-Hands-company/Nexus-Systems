import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { TerminalEngine } from "./terminal-engine";
import { spawnShell, getSession, sessionCount, reapIdle, killAll } from "./pty";
import { callerIdentity, canUseTerminal, terminalEnabled } from "./auth";
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
type TerminalFailureCode = 4003 | 4004 | 1013;
type ShellSocket = {
  kind: "attach";
  subject: string;
  cols: number;
  rows: number;
  remoteIp: string | null;
  sessionId?: string;
} | {
  kind: "reject";
  code: TerminalFailureCode;
};
const TERMINAL_FAILURE_REASONS: Record<TerminalFailureCode, string> = {
  4003: "terminal refused",
  4004: "terminal disabled",
  1013: "session limit reached",
};
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer(options: { auditPath?: string } = {}) { const port = Number(process.env.PORT || "3110"); const baseUrl = process.env.NEXUS_NEXUS_TERMINAL_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new TerminalEngine("data/terminal.sqlite")
  const phantom = new PhantomApp("nexus-terminal");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const audit = new TerminalAudit(options.auditPath);

  // Abandoned tabs leave live shells behind. Without reaping, the session
  // ceiling eventually refuses new ones and nobody can see why.
  const reaper = setInterval(() => { void reapIdle(); }, 60_000);
  let shuttingDown = false;

  const server = Bun.serve<ShellSocket>({ port, hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    websocket: {
      open(ws) {
        if (shuttingDown) {
          ws.close(1012, "terminal shutting down");
          return;
        }
        const d = ws.data;
        if (d.kind === "reject") {
          ws.close(d.code, TERMINAL_FAILURE_REASONS[d.code]);
          return;
        }
        const session = spawnShell({
          subject: d.subject,
          cols: d.cols,
          rows: d.rows,
          onData: (chunk) => { try { ws.send(chunk); } catch { /* socket gone */ } },
          onExit: (id, code) => {
            try { audit.end(id, code); }
            finally { try { ws.close(); } catch { /* already closed */ } }
          },
        });
        if (!session) { ws.close(1013, TERMINAL_FAILURE_REASONS[1013]); return; }
        d.sessionId = session.id;
        try {
          audit.begin(session.id, d.subject, d.remoteIp);
        } catch {
          void session.kill().catch(() => {});
          ws.close(1011, "terminal audit unavailable");
        }
      },
      message(ws, raw) {
        const id = ws.data.kind === "attach" ? ws.data.sessionId : undefined;
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
        const id = ws.data.kind === "attach" ? ws.data.sessionId : undefined;
        if (id) void getSession(id)?.kill();
      },
    },
    async fetch(request, srv) { const url = new URL(request.url); const path = url.pathname || "";

    // ── The shell ────────────────────────────────────────────────────────
    //
    // A WebSocket upgrade carrying a real pty on this host. See
    // docs/TERMINAL-SECURITY.md — the three checks below are the whole of what
    // stands between the internet and a shell here.
    if (path === "/api/v1/terminal/attach") {
      if (shuttingDown) return json({ error: "shutting_down" }, 503);
      // Identity from Auth, asked with the caller's own cookies — never from a
      // header or query parameter the browser chose to send.
      const who = await callerIdentity(request);
      if (!who) return json({ error: "not_authenticated" }, 401);
      if (shuttingDown) return json({ error: "shutting_down" }, 503);
      if (!canUseTerminal(who)) {
        if (srv.upgrade(request, { data: { kind: "reject", code: 4003 } })) {
          return undefined as unknown as Response;
        }
        return json({ error: "forbidden" }, 403);
      }

      // Only an authenticated founder/admin learns operational state. A
      // WebSocket receives a fixed close code; ordinary HTTP keeps the JSON
      // status for diagnostics without exposing an upstream response body.
      if (!terminalEnabled()) {
        if (srv.upgrade(request, { data: { kind: "reject", code: 4004 } })) {
          return undefined as unknown as Response;
        }
        return json({ error: "terminal_disabled" }, 403);
      }

      if (sessionCount() >= 8) {
        if (srv.upgrade(request, { data: { kind: "reject", code: 1013 } })) {
          return undefined as unknown as Response;
        }
        return json({ error: "too_many_sessions" }, 503);
      }

      const cols = Number(url.searchParams.get("cols") || 80);
      const rows = Number(url.searchParams.get("rows") || 24);
      const remoteIp = request.headers.get("x-forwarded-for")?.split(",")[0]?.trim() ?? null;

      if (srv.upgrade(request, { data: { kind: "attach", subject: who.subject, cols, rows, remoteIp } })) {
        return undefined as unknown as Response;
      }
      return json({ error: "upgrade_failed" }, 400);
    }

    if (request.method === "GET" && path === "/api/v1/terminal/audit") {
      const who = await callerIdentity(request);
      if (!who) return json({ error: "not_authenticated" }, 401);
      if (!canUseTerminal(who)) return json({ error: "forbidden" }, 403);
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
  let closePromise: Promise<void> | null = null;
  const close = (): Promise<void> => {
    if (closePromise) return closePromise;
    shuttingDown = true;
    clearInterval(reaper);
    // Bun's graceful-stop promise remains pending for an open WebSocket even
    // after a later forced stop, so it must not be the shutdown completion
    // signal. This call still stops new accepts while shells are finalized.
    void server.stop(false).catch(() => {});
    closePromise = (async () => {
      try {
        await killAll();
        // The shell audit is complete at this point. Force-close any idle HTTP
        // or WebSocket peer that kept Bun's graceful stop promise pending.
        await server.stop(true);
      } finally {
        stopHeartbeat();
        phantom.stop();
        audit.close();
        engine.db.close();
      }
    })();
    return closePromise;
  };
  return { server, engine, close }; }
