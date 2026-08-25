import { afterEach, describe, expect, it } from "bun:test";
import { mkdir, mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import type { TerminalSocketData } from "../../../apps/Nexus-Dashboard/src/terminal";

type Client = {
  socket: WebSocket;
  messages: MessageEvent[];
  opened: Promise<void>;
  closed: Promise<CloseEvent>;
};

const ENVIRONMENT_KEYS = [
  "CLOUD_URL",
  "DASHBOARD_UPSTREAM",
  "DOMAIN",
  "NEXUS_AUTH_INTERNAL_URL",
  "NEXUS_BIND_HOST",
  "NEXUS_DASHBOARD_PUBLIC_URL",
  "NEXUS_DATA_DIR",
  "NEXUS_PROXY_BIND_HOST",
  "NEXUS_TERMINAL_ENABLED",
  "NEXUS_TERMINAL_ENABLE_CLOUD_INTEGRATION",
  "NEXUS_TERMINAL_URL",
  "POLL_INTERVAL_MS",
  "PORT",
  "PROXY_PORT",
  "SHELL",
] as const;
const originalEnvironment = new Map(
  ENVIRONMENT_KEYS.map((key) => [key, process.env[key]] as const),
);

const delay = (milliseconds: number) =>
  new Promise<void>((resolve) => setTimeout(resolve, milliseconds));

async function waitUntil(check: () => boolean, label: string): Promise<void> {
  const deadline = Date.now() + 5_000;
  while (!check()) {
    if (Date.now() >= deadline) throw new Error(`timed out waiting for ${label}`);
    await delay(10);
  }
}

function transcript(messages: MessageEvent[]): string {
  return messages
    .map((message) => (typeof message.data === "string" ? message.data : ""))
    .join("")
    .replaceAll("\r", "")
    .replaceAll(/\x1b\[[0-?]*[ -/]*[@-~]/g, "");
}

function openPublicTerminal(proxyPort: number): Client {
  const socket = new WebSocket(
    `ws://127.0.0.1:${proxyPort}/api/terminal/attach?cols=100&rows=30`,
    {
      headers: {
        host: "app.tnhc.dev",
        origin: "https://app.tnhc.dev",
        cookie: "role=founder; nexus_session=public-hop-session",
      },
    },
  );
  socket.binaryType = "arraybuffer";
  const messages: MessageEvent[] = [];
  socket.addEventListener("message", (event) => messages.push(event));
  const opened = new Promise<void>((resolve, reject) => {
    socket.addEventListener("open", () => resolve(), { once: true });
    socket.addEventListener("error", () => reject(new Error("public terminal did not open")), {
      once: true,
    });
  });
  const closed = new Promise<CloseEvent>((resolve) => {
    socket.addEventListener("close", (event) => resolve(event), { once: true });
  });
  return { socket, messages, opened, closed };
}

async function waitForActiveShells(baseUrl: string, expected: number): Promise<void> {
  let observed: unknown = null;
  const deadline = Date.now() + 5_000;
  while (Date.now() < deadline) {
    const response = await fetch(`${baseUrl}/health`).catch(() => null);
    if (response?.ok) {
      observed = (await response.json() as { activeShells?: unknown }).activeShells;
      if (observed === expected) return;
    }
    await delay(10);
  }
  throw new Error(`timed out waiting for ${expected} active shells; observed ${String(observed)}`);
}

afterEach(() => {
  for (const [key, value] of originalEnvironment) {
    if (value === undefined) Reflect.deleteProperty(process.env, key);
    else process.env[key] = value;
  }
});

describe("production Terminal public hop", () => {
  it("relays proxy to Dashboard to real isolated PTYs with exact Origin and cleanup", async () => {
    const root = await mkdtemp(join(tmpdir(), "nexus-terminal-public-hop."));
    await mkdir(join(root, "data"));
    const previousCwd = process.cwd();
    const authCookies: string[] = [];
    let auth: ReturnType<typeof Bun.serve> | null = null;
    let dashboard: ReturnType<typeof Bun.serve> | null = null;
    let proxy: ReturnType<typeof Bun.serve> | null = null;
    let terminal: Awaited<
      ReturnType<(typeof import("../../../apps/Nexus-Terminal/src/server"))["createServer"]>
    > | null = null;
    const clients: Client[] = [];

    try {
      auth = Bun.serve({
        hostname: "127.0.0.1",
        port: 0,
        fetch(request) {
          const cookie = request.headers.get("cookie") ?? "";
          authCookies.push(cookie);
          if (!cookie.includes("nexus_session=public-hop-session")) {
            return Response.json({ error: "not_authenticated" }, { status: 401 });
          }
          return Response.json({ user: { id: "public-hop-founder", role: "founder" } });
        },
      });

      process.env.DOMAIN = "tnhc.dev";
      process.env.NEXUS_AUTH_INTERNAL_URL = `http://127.0.0.1:${auth.port}`;
      process.env.NEXUS_BIND_HOST = "127.0.0.1";
      process.env.NEXUS_TERMINAL_ENABLED = "true";
      process.env.NEXUS_TERMINAL_ENABLE_CLOUD_INTEGRATION = "false";
      process.env.PORT = "0";
      process.env.SHELL = "/bin/sh";

      process.chdir(root);
      const { createServer } = await import("../../../apps/Nexus-Terminal/src/server");
      terminal = await createServer();
      process.chdir(previousCwd);
      const terminalBaseUrl = `http://127.0.0.1:${terminal.server.port}`;
      process.env.NEXUS_TERMINAL_URL = terminalBaseUrl;
      process.env.NEXUS_DASHBOARD_PUBLIC_URL = "https://app.tnhc.dev";

      const { handleRequest, terminalWebSocketHandlers } = await import(
        "../../../apps/Nexus-Dashboard/src/server"
      ).then(async (serverModule) => ({
        handleRequest: serverModule.handleRequest,
        terminalWebSocketHandlers: (await import(
          "../../../apps/Nexus-Dashboard/src/terminal"
        )).terminalWebSocketHandlers,
      }));
      dashboard = Bun.serve<TerminalSocketData>({
        hostname: "127.0.0.1",
        port: 0,
        websocket: terminalWebSocketHandlers,
        fetch: handleRequest,
      });
      process.env.DASHBOARD_UPSTREAM = `http://127.0.0.1:${dashboard.port}`;

      process.env.PROXY_PORT = "0";
      process.env.POLL_INTERVAL_MS = "0";
      process.env.CLOUD_URL = "http://127.0.0.1:1";
      process.env.NEXUS_PROXY_BIND_HOST = "127.0.0.1";
      const { __setRoutesForTest, startProxy } = await import("../proxy");
      __setRoutesForTest({
        // A mutable route entry must not select the credential-bearing target.
        "app.tnhc.dev": { upstream: "http://client-selected.invalid", requiresAuth: false, kind: "app" },
      });
      proxy = startProxy();

      clients.push(openPublicTerminal(proxy.port), openPublicTerminal(proxy.port));
      await Promise.all(clients.map((client) => client.opened));
      await waitForActiveShells(terminalBaseUrl, 2);
      await Promise.all(clients.map((client, index) =>
        waitUntil(() => transcript(client.messages).includes("$ "), `PTY ${index + 1} prompt`)
      ));

      clients[0]!.messages.length = 0;
      clients[1]!.messages.length = 0;
      clients[0]!.socket.send("printf 'public-hop-one\\n'\r");
      clients[1]!.socket.send("printf 'public-hop-two\\n'\r");
      await waitUntil(
        () => transcript(clients[0]!.messages).includes("\npublic-hop-one\n"),
        "first public-hop output",
      );
      await waitUntil(
        () => transcript(clients[1]!.messages).includes("\npublic-hop-two\n"),
        "second public-hop output",
      );

      expect(transcript(clients[0]!.messages)).not.toContain("public-hop-two");
      expect(transcript(clients[1]!.messages)).not.toContain("public-hop-one");
      expect(authCookies.length).toBeGreaterThanOrEqual(4);
      expect(authCookies.every((cookie) => cookie.includes("public-hop-session"))).toBe(true);

      for (const client of clients) client.socket.close();
      await Promise.all(clients.map((client) => client.closed));
      await waitForActiveShells(terminalBaseUrl, 0);
    } finally {
      process.chdir(previousCwd);
      for (const client of clients) {
        if (client.socket.readyState < WebSocket.CLOSING) client.socket.terminate();
      }
      await proxy?.stop(true);
      await dashboard?.stop(true);
      await terminal?.close();
      await auth?.stop(true);
      await rm(root, { recursive: true, force: true });
    }
  });
});
