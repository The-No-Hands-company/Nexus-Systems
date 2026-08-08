import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createTeamChatServer } from "../src/server";

describe("nexus-team-chat", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createTeamChatServer>>;

  beforeAll(async () => {
    handle = await createTeamChatServer();
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const r = await fetch(`${base}/health`);
    expect(r.status).toBe(200);
    const body = await r.json() as { service: string };
    expect(body.service).toBe("nexus-team-chat");
  });

  it("GET /api/v1/chat/status returns channels count", async () => {
    const r = await fetch(`${base}/api/v1/chat/status`);
    expect(r.status).toBe(200);
    const body = await r.json() as { channels: number };
    expect(body.channels).toBeGreaterThan(0);
  });

  it("WebSocket: connects and receives Hello", async () => {
    const ws = new WebSocket(`ws://127.0.0.1:${handle.server.port}/gateway`);
    const msg = await new Promise<{ op: string }>((resolve) => {
      ws.onmessage = (e) => resolve(JSON.parse(e.data as string));
      ws.onerror = () => resolve({ op: "error" });
    });
    expect(msg.op).toBe("Hello");
    ws.close();
  });

  it("WebSocket: Hello → Identify → Ready → CreateMessage → MESSAGE_CREATE", async () => {
    const ws = new WebSocket(`ws://127.0.0.1:${handle.server.port}/gateway`);
    const events: string[] = [];

    await new Promise<void>((resolve) => {
      ws.onmessage = (e) => {
        const p = JSON.parse(e.data as string);
        events.push(p.op === "Dispatch" ? `Dispatch:${p.t}` : p.op);

        if (p.op === "Hello") {
          ws.send(JSON.stringify({ op: "Identify", d: { token: "test-token-123" } }));
        } else if (p.op === "Ready") {
          expect(p.d.user).toBeDefined();
          expect(Array.isArray(p.d.channels)).toBe(true);
          expect(p.d.channels.length).toBeGreaterThan(0);

          // Send a message
          const channelId = p.d.channels[0].id;
          ws.send(JSON.stringify({ op: "CreateMessage", d: { channelId, content: "Hello from test!" } }));
        } else if (p.op === "Dispatch" && p.t === "MESSAGE_CREATE") {
          expect(p.d.content).toBe("Hello from test!");
          resolve();
        }
      };
    });

    expect(events).toContain("Hello");
    expect(events).toContain("Ready");
    expect(events.some((e) => e.startsWith("Dispatch:MESSAGE_CREATE"))).toBe(true);
    ws.close();
  });

  it("WebSocket: typing indicator", async () => {
    const ws = new WebSocket(`ws://127.0.0.1:${handle.server.port}/gateway`);

    await new Promise<void>((resolve) => {
      ws.onmessage = (e) => {
        const p = JSON.parse(e.data as string);
        if (p.op === "Hello") {
          ws.send(JSON.stringify({ op: "Identify", d: { token: "test-typer" } }));
        } else if (p.op === "Ready") {
          ws.send(JSON.stringify({ op: "TypingStart", d: { channelId: "general" } }));
        } else if (p.op === "Dispatch" && p.t === "TYPING_START") {
          resolve();
        }
      };
    });

    ws.close();
  });

  it("WebSocket: heartbeat exchange", async () => {
    const ws = new WebSocket(`ws://127.0.0.1:${handle.server.port}/gateway`);

    await new Promise<void>((resolve) => {
      ws.onmessage = (e) => {
        const p = JSON.parse(e.data as string);
        if (p.op === "Hello") {
          ws.send(JSON.stringify({ op: "Heartbeat", d: { timestamp: Date.now() } }));
        } else if (p.op === "HeartbeatAck") {
          resolve();
        }
      };
    });

    ws.close();
  });
});
