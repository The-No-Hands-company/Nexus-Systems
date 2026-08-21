import { afterAll, afterEach, beforeAll, beforeEach, describe, expect, it } from "bun:test";
import { handleRequest } from "../src/server";
import {
  type TerminalSocketData,
  authorizeTerminalUpgrade,
  terminalWebSocketHandlers,
} from "../src/terminal";

type BunServer = ReturnType<typeof Bun.serve>;

const AUTH_PORT = 4399;
const TERMINAL_PORT = 4397;

let auth: BunServer;
let dashboard: BunServer;
let terminal: BunServer | null = null;
let upstreamConnections = 0;
let upstreamActive = 0;
let upstreamHandshakeFinished = false;
let upstreamCookie: string | null = null;
let upstreamPath: string | null = null;
let upstreamSocket: Bun.ServerWebSocket<undefined> | null = null;
let upstreamClose: { code: number; reason: string } | null = null;
let authRequests = 0;

const delay = (milliseconds: number) =>
  new Promise<void>((resolve) => setTimeout(resolve, milliseconds));

async function waitUntil(check: () => boolean, label: string): Promise<void> {
  const deadline = Date.now() + 2_000;
  while (!check()) {
    if (Date.now() >= deadline) throw new Error(`timed out waiting for ${label}`);
    await delay(5);
  }
}

function request(role: string | null, path = "/api/terminal/attach?cols=120&rows=40"): Request {
  const headers = new Headers({
    connection: "Upgrade",
    upgrade: "websocket",
    "sec-websocket-key": "dGhlIHNhbXBsZSBub25jZQ==",
    "sec-websocket-version": "13",
    origin: `http://127.0.0.1:${dashboard.port}`,
  });
  if (role !== null) headers.set("cookie", `role=${role}; nexus_session=secret-session`);
  return new Request(`http://app.test${path}`, { headers });
}

function openClient(
  role: string | null,
  origin: string | null = `http://127.0.0.1:${dashboard.port}`,
): {
  socket: WebSocket;
  messages: MessageEvent[];
  opened: Promise<void>;
  closed: Promise<CloseEvent>;
} {
  const headers: Record<string, string> = {};
  if (role !== null) headers.cookie = `role=${role}; nexus_session=secret-session`;
  if (origin !== null) headers.origin = origin;
  const socket = new WebSocket(
    `ws://127.0.0.1:${dashboard.port}/api/terminal/attach?cols=120&rows=40`,
    { headers },
  );
  socket.binaryType = "arraybuffer";
  const messages: MessageEvent[] = [];
  socket.addEventListener("message", (event) => messages.push(event));
  const opened = new Promise<void>((resolve, reject) => {
    socket.addEventListener("open", () => resolve(), { once: true });
    socket.addEventListener("error", () => reject(new Error("websocket failed to open")), {
      once: true,
    });
  });
  // Rejected handshakes intentionally leave `opened` un-awaited in the
  // authorization tests; attach a handler so that expected rejection is not
  // reported as an unrelated unhandled promise between tests.
  void opened.catch(() => {});
  const closed = new Promise<CloseEvent>((resolve) => {
    socket.addEventListener("close", (event) => resolve(event), { once: true });
  });
  return { socket, messages, opened, closed };
}

function startTerminal(options: { handshakeDelay?: number } = {}): void {
  terminal = Bun.serve<undefined>({
    hostname: "127.0.0.1",
    port: TERMINAL_PORT,
    websocket: {
      open(ws) {
        upstreamConnections++;
        upstreamActive++;
        upstreamSocket = ws;
      },
      message(ws, message) {
        ws.send(message);
      },
      close(_ws, code, reason) {
        upstreamActive--;
        upstreamClose = { code, reason };
      },
    },
    async fetch(req, server) {
      const url = new URL(req.url);
      upstreamCookie = req.headers.get("cookie");
      upstreamPath = `${url.pathname}${url.search}`;
      if (options.handshakeDelay) await delay(options.handshakeDelay);
      const upgraded =
        url.pathname === "/api/v1/terminal/attach" && server.upgrade(req, { data: undefined });
      upstreamHandshakeFinished = true;
      if (upgraded) {
        return undefined;
      }
      return new Response("not found", { status: 404 });
    },
  });
}

beforeAll(() => {
  process.env.NEXUS_AUTH_INTERNAL_URL = `http://127.0.0.1:${AUTH_PORT}`;
  process.env.NEXUS_TERMINAL_URL = `http://127.0.0.1:${TERMINAL_PORT}`;

  auth = Bun.serve({
    hostname: "127.0.0.1",
    port: AUTH_PORT,
    fetch(req) {
      authRequests++;
      const role = req.headers.get("cookie")?.match(/(?:^|;\s*)role=([^;]+)/)?.[1];
      if (role === "outage") return Response.json({ error: "unavailable" }, { status: 503 });
      if (!role) return Response.json({ error: "not_authenticated" }, { status: 401 });
      return Response.json({ user: { id: `user-${role}`, role } });
    },
  });

  dashboard = Bun.serve<TerminalSocketData>({
    hostname: "127.0.0.1",
    port: 0,
    websocket: terminalWebSocketHandlers,
    fetch: handleRequest,
  });
  process.env.NEXUS_DASHBOARD_PUBLIC_URL = `http://127.0.0.1:${dashboard.port}`;
  process.env.NEXUS_TERMINAL_CONNECT_TIMEOUT_MS = "100";
});

beforeEach(() => {
  upstreamConnections = 0;
  upstreamActive = 0;
  upstreamHandshakeFinished = false;
  upstreamCookie = null;
  upstreamPath = null;
  upstreamSocket = null;
  upstreamClose = null;
  authRequests = 0;
  startTerminal();
});

afterEach(() => {
  terminal?.stop(true);
  terminal = null;
});

afterAll(() => {
  dashboard.stop(true);
  auth.stop(true);
});

describe("terminal upgrade authorization", () => {
  it("refuses signed-out and member callers before Terminal is contacted", async () => {
    for (const role of [null, "member"] as const) {
      const client = openClient(role);
      await client.closed;
    }

    expect(upstreamConnections).toBe(0);
    expect(upstreamPath).toBeNull();
  });

  it("rejects missing and untrusted origins before Auth or Terminal is contacted", async () => {
    for (const origin of [null, "https://other.tnhc.dev"] as const) {
      const client = openClient("founder", origin);
      const outcome = await Promise.race([
        client.opened.then(
          () => "opened" as const,
          () => "rejected" as const,
        ),
        client.closed.then(() => "closed" as const),
      ]);
      if (outcome === "opened") {
        client.socket.close();
        await client.closed;
      }
      expect(outcome).not.toBe("opened");
    }

    expect(authRequests).toBe(0);
    expect(upstreamConnections).toBe(0);
  });

  it("builds the one configured attach URL for founders and admins", async () => {
    for (const role of ["founder", "admin"] as const) {
      const result = await authorizeTerminalUpgrade(request(role));
      expect(result.ok).toBe(true);
      if (result.ok) {
        expect(result.data.upstreamUrl).toBe(
          `ws://127.0.0.1:${TERMINAL_PORT}/api/v1/terminal/attach?cols=120&rows=40`,
        );
      }
    }
  });

  it("ignores a client-selected upstream and fails closed when Auth is unavailable", async () => {
    const injected = await authorizeTerminalUpgrade(
      request("founder", "/api/terminal/attach?cols=120&rows=40&upstream=ws%3A%2F%2Fevil.example"),
    );
    expect(injected.ok).toBe(true);
    if (injected.ok) {
      expect(injected.data.upstreamUrl).toBe(
        `ws://127.0.0.1:${TERMINAL_PORT}/api/v1/terminal/attach?cols=120&rows=40`,
      );
    }

    const unavailable = openClient("outage");
    await unavailable.closed;
    expect(upstreamConnections).toBe(0);
  });

  it("refuses every non-WebSocket request and every other path", async () => {
    const plain = await authorizeTerminalUpgrade(
      new Request("http://app.test/api/terminal/attach", {
        headers: { cookie: "role=founder; nexus_session=secret-session" },
      }),
    );
    const otherPath = await authorizeTerminalUpgrade(
      request("founder", "/api/terminal/attach/extra?cols=120&rows=40"),
    );

    expect(plain.ok).toBe(false);
    expect(otherPath.ok).toBe(false);
  });

  it("clamps dimensions to Nexus-Terminal bounds and rejects non-numbers", async () => {
    const clamped = await authorizeTerminalUpgrade(
      request("founder", "/api/terminal/attach?cols=9000&rows=1"),
    );
    expect(clamped.ok).toBe(true);
    if (clamped.ok) {
      expect(clamped.data.upstreamUrl).toEndWith("?cols=500&rows=5");
    }

    const invalid = await authorizeTerminalUpgrade(
      request("founder", "/api/terminal/attach?cols=wide&rows=40"),
    );
    expect(invalid.ok).toBe(false);
    if (!invalid.ok) expect(invalid.response.status).toBe(400);
  });
});

describe("terminal frame relay", () => {
  it("forwards the cookie only in the fixed upstream handshake", async () => {
    const client = openClient("founder");
    await client.opened;
    await waitUntil(() => upstreamConnections === 1, "upstream connection");

    expect(upstreamCookie).toBe("role=founder; nexus_session=secret-session");
    expect(upstreamPath).toBe("/api/v1/terminal/attach?cols=120&rows=40");
    expect(upstreamPath).not.toContain("secret-session");
    client.socket.close();
    await client.closed;
  });

  it("buffers browser frames sent before the upstream opens", async () => {
    terminal?.stop(true);
    startTerminal({ handshakeDelay: 75 });
    const client = openClient("founder");
    await client.opened;
    client.socket.send("early input");

    await waitUntil(() => client.messages.length === 1, "buffered frame echo");
    expect(client.messages[0]?.data).toBe("early input");
    client.socket.close();
    await client.closed;
  });

  it("relays text and binary frames without changing their type", async () => {
    const client = openClient("admin");
    await client.opened;
    client.socket.send("hello terminal");
    client.socket.send(new Uint8Array([0, 1, 2, 255]));

    await waitUntil(() => client.messages.length === 2, "text and binary echoes");
    expect(client.messages[0]?.data).toBe("hello terminal");
    expect(new Uint8Array(client.messages[1]?.data as ArrayBuffer)).toEqual(
      new Uint8Array([0, 1, 2, 255]),
    );
    client.socket.close();
    await client.closed;
  });

  it("propagates upstream and browser closes to the paired socket", async () => {
    const fromUpstream = openClient("founder");
    await fromUpstream.opened;
    await waitUntil(() => upstreamSocket !== null, "upstream socket");
    upstreamSocket?.close(4001, "pty exited");
    const browserClose = await fromUpstream.closed;
    expect(browserClose.code).toBe(4001);
    expect(browserClose.reason).toBe("pty exited");

    upstreamClose = null;
    upstreamSocket = null;
    const fromBrowser = openClient("founder");
    await fromBrowser.opened;
    await waitUntil(() => upstreamSocket !== null, "second upstream socket");
    fromBrowser.socket.close(4002, "tab closed");
    await waitUntil(() => upstreamClose !== null, "upstream close");
    // Bun 1.3.12 preserves the client close code in its server callback but
    // currently supplies an empty reason. The relay forwards exactly what its
    // server-side callback observes.
    expect(upstreamClose as { code: number; reason: string } | null).toEqual({
      code: 4002,
      reason: "",
    });
  });

  it("does not leave an upstream socket when the browser closes during its handshake", async () => {
    terminal?.stop(true);
    startTerminal({ handshakeDelay: 75 });
    const client = openClient("founder");
    await client.opened;
    client.socket.close();
    await client.closed;

    await waitUntil(() => upstreamHandshakeFinished, "delayed upstream handshake");
    // Let Bun deliver any open/close callbacks queued by the completed
    // handshake. The assertion is the durable state, not callback ordering.
    await delay(20);
    expect(upstreamActive).toBe(0);
  });

  it("terminates the upstream after an abrupt browser disconnect", async () => {
    const client = openClient("founder");
    await client.opened;
    await waitUntil(() => upstreamActive === 1, "active upstream socket");
    client.socket.terminate();
    await client.closed;

    await waitUntil(() => upstreamActive === 0, "abrupt upstream cleanup");
  });

  it("maps an abrupt upstream disconnect to 1011", async () => {
    const client = openClient("founder");
    await client.opened;
    await waitUntil(() => upstreamSocket !== null, "upstream socket before termination");
    upstreamSocket?.terminate();

    const close = await client.closed;
    expect(close.code).toBe(1011);
  });

  it("closes on a stalled upstream handshake instead of waiting indefinitely", async () => {
    terminal?.stop(true);
    startTerminal({ handshakeDelay: 5_000 });
    const client = openClient("founder");
    await client.opened;

    const close = await Promise.race([client.closed, delay(500).then(() => null)]);
    expect(close?.code).toBe(1011);
  });

  it("bounds the aggregate frame queue while the upstream is opening", async () => {
    terminal?.stop(true);
    startTerminal({ handshakeDelay: 5_000 });
    const client = openClient("founder");
    await client.opened;
    client.socket.send(new Uint8Array(700 * 1024));
    client.socket.send(new Uint8Array(700 * 1024));

    const close = await Promise.race([client.closed, delay(500).then(() => null)]);
    expect(close?.code).toBe(1011);
  });

  it("closes the browser with 1011 when the upstream cannot open", async () => {
    terminal?.stop(true);
    terminal = null;
    const client = openClient("founder");
    await client.opened;

    const close = await client.closed;
    expect(close.code).toBe(1011);
  });
});
