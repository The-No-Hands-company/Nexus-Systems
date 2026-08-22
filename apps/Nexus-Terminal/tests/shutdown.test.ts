import { Database } from "bun:sqlite";
import { afterEach, describe, expect, it } from "bun:test";
import { mkdtemp, rm } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { killAll } from "../src/pty";
import { createServer } from "../src/server";

type ServerHandle = Awaited<ReturnType<typeof createServer>>;

const environmentKeys = [
  "NEXUS_AUTH_INTERNAL_URL",
  "NEXUS_BIND_HOST",
  "NEXUS_DATA_DIR",
  "NEXUS_TERMINAL_ENABLED",
  "NEXUS_TERMINAL_ENABLE_CLOUD_INTEGRATION",
  "PORT",
  "SHELL",
] as const;
const originalEnvironment = new Map(
  environmentKeys.map((key) => [key, process.env[key]] as const),
);

async function waitForActiveShells(baseUrl: string, expected: number): Promise<void> {
  const deadline = Date.now() + 3_000;
  while (Date.now() < deadline) {
    const response = await fetch(`${baseUrl}/health`).catch(() => null);
    if (response?.ok) {
      const body = await response.json() as { activeShells?: number };
      if (body.activeShells === expected) return;
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw new Error(`timed out waiting for ${expected} active shells`);
}

function openTerminal(baseUrl: string): { socket: WebSocket; opened: Promise<void> } {
  const socket = new WebSocket(
    `${baseUrl.replace(/^http/, "ws")}/api/v1/terminal/attach?cols=80&rows=24`,
    { headers: { cookie: "role=founder" } },
  );
  const opened = new Promise<void>((resolve, reject) => {
    socket.addEventListener("open", () => resolve(), { once: true });
    socket.addEventListener("error", () => reject(new Error("terminal websocket failed")), {
      once: true,
    });
  });
  return { socket, opened };
}

afterEach(async () => {
  await killAll();
  for (const [key, value] of originalEnvironment) {
    if (value === undefined) Reflect.deleteProperty(process.env, key);
    else process.env[key] = value;
  }
});

describe("Terminal server shutdown", () => {
  it("is async and idempotent, and persists audit end before resolving", async () => {
    const root = await mkdtemp(join(tmpdir(), "nexus-terminal-shutdown."));
    const auditPath = join(root, "terminal-audit.sqlite");
    let auth: ReturnType<typeof Bun.serve> | null = null;
    let handle: ServerHandle | null = null;
    let socket: WebSocket | null = null;

    try {
      auth = Bun.serve({
        hostname: "127.0.0.1",
        port: 0,
        fetch() {
          return Response.json({ user: { id: "shutdown-founder", role: "founder" } });
        },
      });
      process.env.NEXUS_AUTH_INTERNAL_URL = `http://127.0.0.1:${auth.port}`;
      process.env.NEXUS_DATA_DIR = root;
      process.env.NEXUS_TERMINAL_ENABLED = "true";
      process.env.NEXUS_TERMINAL_ENABLE_CLOUD_INTEGRATION = "false";
      process.env.PORT = "0";
      process.env.SHELL = "/bin/sh";

      const createWithAuditPath = createServer as unknown as (
        options: { auditPath: string },
      ) => Promise<ServerHandle>;
      handle = await createWithAuditPath({ auditPath });
      const baseUrl = `http://127.0.0.1:${handle.server.port}`;
      const client = openTerminal(baseUrl);
      socket = client.socket;
      await client.opened;
      await waitForActiveShells(baseUrl, 1);

      const closing = handle.close();
      const closingAgain = handle.close();

      expect(closing).toBeInstanceOf(Promise);
      expect(closingAgain).toBe(closing);
      await closing;

      const db = new Database(auditPath, { readonly: true });
      const record = db
        .query("SELECT subject, ended_at, exit_code FROM sessions LIMIT 1")
        .get() as { subject: string; ended_at: string | null; exit_code: number | null } | null;
      db.close();
      expect(record?.subject).toBe("shutdown-founder");
      expect(typeof record?.ended_at).toBe("string");
      expect(typeof record?.exit_code).toBe("number");
    } finally {
      if (socket && socket.readyState < WebSocket.CLOSING) socket.terminate();
      await handle?.close();
      auth?.stop(true);
      await rm(root, { recursive: true, force: true });
    }
  });
});
