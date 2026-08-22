import { callerIdentity, isAdminRole } from "./auth";

const ATTACH_PATH = "/api/terminal/attach";
const UPSTREAM_ATTACH_PATH = "/api/v1/terminal/attach";
const DEFAULT_TERMINAL_URL = "http://127.0.0.1:3110";

const COLS = { default: 80, min: 20, max: 500 } as const;
const ROWS = { default: 24, min: 5, max: 200 } as const;
const MAX_PENDING_BYTES = 1024 * 1024;

type TerminalFailureCode = 4003 | 4004 | 1013;
type TerminalRelayData = { kind: "relay"; upstreamUrl: string; cookie: string };
type TerminalRejectData = { kind: "reject"; code: TerminalFailureCode };
export type TerminalSocketData = TerminalRelayData | TerminalRejectData;
export type UpgradeDecision =
  | { ok: true; data: TerminalRelayData }
  | { ok: false; response: Response; failure?: TerminalRejectData };

const TERMINAL_FAILURE_REASONS: Record<TerminalFailureCode, string> = {
  4003: "terminal refused",
  4004: "terminal disabled",
  1013: "session limit reached",
};

type RelayFrame = string | Uint8Array;
type RelayPair = {
  upstream: WebSocket;
  queued: RelayFrame[];
  queuedBytes: number;
  upstreamOpen: boolean;
  closed: boolean;
  openTimer: ReturnType<typeof setTimeout> | null;
};

const relayPairs = new WeakMap<Bun.ServerWebSocket<TerminalSocketData>, RelayPair>();

function errorResponse(error: string, status: number): Response {
  return Response.json({ error }, { status });
}

function hasWebSocketHeaders(req: Request): boolean {
  const connection = req.headers
    .get("connection")
    ?.split(",")
    .some((value) => value.trim().toLowerCase() === "upgrade");
  return (
    req.method === "GET" &&
    connection === true &&
    req.headers.get("upgrade")?.toLowerCase() === "websocket" &&
    !!req.headers.get("sec-websocket-key") &&
    req.headers.get("sec-websocket-version") === "13"
  );
}

function dashboardOrigin(): string | null {
  try {
    const url = new URL(
      process.env.NEXUS_DASHBOARD_PUBLIC_URL || `https://app.${process.env.DOMAIN || "tnhc.dev"}`,
    );
    if (url.username || url.password) return null;
    if (url.protocol !== "http:" && url.protocol !== "https:") return null;
    return url.origin;
  } catch {
    return null;
  }
}

function dimension(
  url: URL,
  name: "cols" | "rows",
  bounds: { default: number; min: number; max: number },
): number | null {
  const raw = url.searchParams.get(name);
  if (raw === null) return bounds.default;
  if (!raw.trim()) return null;

  const value = Number(raw);
  if (!Number.isFinite(value)) return null;
  return Math.max(bounds.min, Math.min(bounds.max, Math.trunc(value)));
}

function upstreamUrl(cols: number, rows: number): string | null {
  try {
    const url = new URL(process.env.NEXUS_TERMINAL_URL || DEFAULT_TERMINAL_URL);
    if (url.username || url.password) return null;
    if (url.protocol === "http:") url.protocol = "ws:";
    else if (url.protocol === "https:") url.protocol = "wss:";
    else if (url.protocol !== "ws:" && url.protocol !== "wss:") return null;

    url.pathname = UPSTREAM_ATTACH_PATH;
    url.search = "";
    url.hash = "";
    url.searchParams.set("cols", String(cols));
    url.searchParams.set("rows", String(rows));
    return url.toString();
  } catch {
    return null;
  }
}

function relayCloseCode(code: number): number {
  const standard = code >= 1000 && code <= 1014 && ![1004, 1005, 1006].includes(code);
  const application = code >= 3000 && code <= 4999;
  return standard || application ? code : 1011;
}

function relayCloseReason(reason: string): string {
  return new TextEncoder().encode(reason).byteLength <= 123 ? reason : "";
}

function safeUpstreamClose(code: number): { code: number; reason: string } {
  if (code === 4003 || code === 4004 || code === 1013) {
    return { code, reason: TERMINAL_FAILURE_REASONS[code] };
  }
  if (code === 1000 || code === 1001) return { code, reason: "" };
  return { code: 1011, reason: "terminal upstream unavailable" };
}

function terminalConnectTimeoutMs(): number {
  const configured = Number(process.env.NEXUS_TERMINAL_CONNECT_TIMEOUT_MS || "5000");
  if (!Number.isFinite(configured)) return 5_000;
  return Math.max(100, Math.min(30_000, Math.trunc(configured)));
}

function frameBytes(frame: RelayFrame): number {
  return typeof frame === "string" ? new TextEncoder().encode(frame).byteLength : frame.byteLength;
}

function clearOpenTimer(pair: RelayPair): void {
  if (pair.openTimer !== null) clearTimeout(pair.openTimer);
  pair.openTimer = null;
}

/**
 * Authorize the Dashboard side of the one terminal attach route.
 *
 * Nexus-Terminal repeats this check independently before it creates a PTY.
 */
export async function authorizeTerminalUpgrade(req: Request): Promise<UpgradeDecision> {
  const url = new URL(req.url);
  if (url.pathname !== ATTACH_PATH) {
    return { ok: false, response: errorResponse("not_found", 404) };
  }
  if (!hasWebSocketHeaders(req)) {
    return { ok: false, response: errorResponse("websocket_upgrade_required", 400) };
  }
  const allowedOrigin = dashboardOrigin();
  if (!allowedOrigin || req.headers.get("origin") !== allowedOrigin) {
    return { ok: false, response: errorResponse("origin_not_allowed", 403) };
  }

  const cookie = req.headers.get("cookie")?.trim() ?? "";
  if (!cookie) {
    return { ok: false, response: errorResponse("not_authenticated", 401) };
  }

  const caller = await callerIdentity(req);
  if (!caller) {
    return { ok: false, response: errorResponse("not_authenticated", 401) };
  }
  if (!isAdminRole(caller.role)) {
    return {
      ok: false,
      response: errorResponse("forbidden", 403),
      failure: { kind: "reject", code: 4003 },
    };
  }

  const cols = dimension(url, "cols", COLS);
  const rows = dimension(url, "rows", ROWS);
  if (cols === null || rows === null) {
    return { ok: false, response: errorResponse("invalid_dimensions", 400) };
  }

  const fixedUpstream = upstreamUrl(cols, rows);
  if (!fixedUpstream) {
    return { ok: false, response: errorResponse("terminal_unavailable", 503) };
  }
  return { ok: true, data: { kind: "relay", upstreamUrl: fixedUpstream, cookie } };
}

function closePair(
  ws: Bun.ServerWebSocket<TerminalSocketData>,
  pair: RelayPair,
  code: number,
  reason: string,
  source: "browser" | "upstream",
): void {
  if (pair.closed) return;
  pair.closed = true;
  clearOpenTimer(pair);
  pair.queued.length = 0;
  pair.queuedBytes = 0;
  relayPairs.delete(ws);

  const safeClose = source === "upstream"
    ? safeUpstreamClose(code)
    : { code: relayCloseCode(code), reason: relayCloseReason(reason) };
  try {
    if (source === "browser") pair.upstream.close(safeClose.code, safeClose.reason);
    else ws.close(safeClose.code, safeClose.reason);
  } catch {
    // Reserved codes and close races must not leave the peer alive.
    if (source === "browser") pair.upstream.terminate();
    else ws.terminate();
  }
}

function failPair(ws: Bun.ServerWebSocket<TerminalSocketData>, pair: RelayPair): void {
  if (pair.closed) return;
  pair.closed = true;
  clearOpenTimer(pair);
  pair.queued.length = 0;
  pair.queuedBytes = 0;
  relayPairs.delete(ws);
  try {
    pair.upstream.terminate();
  } catch {
    // A failed connection may never have reached a closable state.
  }
  try {
    ws.close(1011, "terminal upstream unavailable");
  } catch {
    // The browser may have closed at the same time as the upstream failed.
  }
}

export const terminalWebSocketHandlers: Bun.WebSocketHandler<TerminalSocketData> = {
  open(ws) {
    const data = ws.data;
    if (data.kind === "reject") {
      ws.close(data.code, TERMINAL_FAILURE_REASONS[data.code]);
      return;
    }
    let upstream: WebSocket;
    try {
      // The session cookie is deliberately confined to the loopback
      // handshake headers. It never enters the URL, a frame, or a log.
      upstream = new WebSocket(data.upstreamUrl, {
        headers: { cookie: data.cookie },
      });
      upstream.binaryType = "arraybuffer";
    } catch {
      try {
        ws.close(1011, "terminal upstream unavailable");
      } catch {
        // The upgraded browser socket may already be gone.
      }
      return;
    }

    const pair: RelayPair = {
      upstream,
      queued: [],
      queuedBytes: 0,
      upstreamOpen: false,
      closed: false,
      openTimer: null,
    };
    relayPairs.set(ws, pair);
    pair.openTimer = setTimeout(() => failPair(ws, pair), terminalConnectTimeoutMs());

    upstream.addEventListener("open", () => {
      if (pair.closed) return;
      clearOpenTimer(pair);
      pair.upstreamOpen = true;
      try {
        for (const frame of pair.queued) upstream.send(frame);
        pair.queued.length = 0;
        pair.queuedBytes = 0;
      } catch {
        failPair(ws, pair);
      }
    });
    upstream.addEventListener("message", (event) => {
      if (pair.closed) return;
      try {
        if (typeof event.data === "string" || event.data instanceof ArrayBuffer) {
          ws.send(event.data);
        } else {
          failPair(ws, pair);
        }
      } catch {
        failPair(ws, pair);
      }
    });
    upstream.addEventListener("close", (event) => {
      closePair(ws, pair, event.code, event.reason, "upstream");
    });
    upstream.addEventListener("error", () => failPair(ws, pair));
  },

  message(ws, message) {
    const pair = relayPairs.get(ws);
    if (!pair || pair.closed) return;
    const frame = typeof message === "string" ? message : new Uint8Array(message);
    if (!pair.upstreamOpen) {
      const bytes = frameBytes(frame);
      if (pair.queuedBytes + bytes > MAX_PENDING_BYTES) {
        failPair(ws, pair);
        return;
      }
      pair.queued.push(frame);
      pair.queuedBytes += bytes;
      return;
    }
    try {
      pair.upstream.send(frame);
    } catch {
      failPair(ws, pair);
    }
  },

  close(ws, code, reason) {
    const pair = relayPairs.get(ws);
    if (pair) closePair(ws, pair, code, reason, "browser");
  },

  maxPayloadLength: 1024 * 1024,
  backpressureLimit: 1024 * 1024,
  closeOnBackpressureLimit: true,
};
