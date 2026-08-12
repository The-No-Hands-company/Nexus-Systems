import { ChatEngine, type GatewayPayload, type ReadyData } from "./chat-engine";

import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { startNexusTeamChatHeartbeat } from "./cloud";
import { NexusClient, createConfig } from "../../../packages/nexus-sdk/src/index";

const HEARTBEAT_INTERVAL = 45000;
const IDENTIFY_TIMEOUT = 10000;

interface WsClient {
  userId: string | undefined;
  username: string | undefined;
  sessionId: string;
  channels: string[];
  lastHeartbeat: number;
}

function send(ws: { send: (data: string) => void }, payload: GatewayPayload): void {
  ws.send(JSON.stringify(payload));
}

async function verifyToken(token: string): Promise<{ userId: string; username: string } | null> {
  const authUrl = process.env.NEXUS_AUTH_URL || "http://localhost:4310";
  try {
    const r = await fetch(`${authUrl}/api/v1/auth/trust`, {
      headers: { authorization: `Bearer ${token}` },
      signal: AbortSignal.timeout(3000),
    });
    if (!r.ok) return null;
    const data = await r.json() as { trusted: boolean; userId?: string; user?: { username: string } };
    return data.trusted ? { userId: data.userId || "", username: data.user?.username || "unknown" } : null;
  } catch {
    // Fallback: allow with generated ID for dev
    const devId = `user-${token.slice(0, 8)}`;
    return { userId: devId, username: devId };
  }
}

export async function createTeamChatServer() {
  const port = Number(process.env.PORT || "3109");
  const baseUrl = process.env.NEXUS_TEAM_CHAT_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new ChatEngine(process.env.NEXUS_TEAM_CHAT_DB_PATH || "data/team-chat.sqlite")
  const phantom = new PhantomApp("nexus-team-chat");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const clients = new Map<WsClient, { send: (data: string) => void }>();

  // Seed default channel
  if (engine.getChannels().length === 0) {
    engine.createChannel({ name: "general", description: "General discussion" });
    engine.createChannel({ name: "random", description: "Off-topic chat" });
  }

  // Broadcast events to subscribed clients
  engine.onDispatch((event) => {
    const payload: GatewayPayload = { op: "Dispatch", t: event.event, d: event.data };
    for (const [client, ws] of clients) {
      if (!event.channelId || client.channels.includes(event.channelId)) {
        try { send(ws, payload); } catch {}
      }
    }
  });

  
const nexusClient = new NexusClient(createConfig({
  id: "nexus-team-chat",
  name: "Nexus Team Chat",
  description: "Real-time team messaging",
  port: 3109,
  capabilities: ["websocket-gateway","channels","messaging","typing-indicators","presence","realtime"],
}));

const stopNexusHeartbeat = nexusClient.startCloudHeartbeat();
const stopNexusMonitor = nexusClient.startMonitorHeartbeat();
const server = Bun.serve({
    port,
    // Loopback unless told otherwise — Bun.serve binds every interface by
    // default, and the proxy is the only intended client.
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    websocket: {
      open(ws) {
        const sessionId = crypto.randomUUID?.() || Math.random().toString(36).slice(2);
        const client: WsClient = { userId: undefined, username: undefined, sessionId, channels: [], lastHeartbeat: Date.now() };
        clients.set(client, ws);

        send(ws, { op: "Hello", d: { heartbeat_interval: HEARTBEAT_INTERVAL } });

        setTimeout(() => {
          if (!client.userId && clients.has(client)) {
            send(ws, { op: "Reconnect", d: { reason: "Identify timeout" } });
            clients.delete(client);
          }
        }, IDENTIFY_TIMEOUT);
      },

      async message(ws, raw) {
        const data = raw as string;
        let payload: GatewayPayload;
        try { payload = JSON.parse(data); } catch { return; }

        let client: WsClient | undefined;
        for (const [c, w] of clients) { if (w === ws) { client = c; break; } }
        if (!client) return;

        const d = payload.d as Record<string, unknown> | undefined;

        switch (payload.op) {
          case "Identify": {
            const token = d?.token as string;
            const auth = await verifyToken(token);
            if (!auth) { send(ws, { op: "Reconnect", d: { reason: "Invalid token" } }); break; }

            client.userId = auth.userId;
            client.username = auth.username;

            const channels = engine.getChannels();
            const messages: Record<string, unknown> = {};
            const membersMap: Record<string, unknown> = {};

            for (const ch of channels) {
              messages[ch.id] = engine.getMessages(ch.id, 30).reverse();
              membersMap[ch.id] = engine.getMembers(ch.id);
              engine.joinChannel(auth.userId, auth.username, ch.id);
              client!.channels.push(ch.id);
            }

            const readyData: ReadyData = {
              user: { id: auth.userId, username: auth.username },
              channels,
              messages: messages as ReadyData["messages"],
              members: membersMap as ReadyData["members"],
            };
            send(ws, { op: "Ready", d: readyData });

            send(ws, { op: "Dispatch", t: "PRESENCE_UPDATE", d: { userId: auth.userId, status: "online" } });
            break;
          }

          case "Heartbeat":
            client.lastHeartbeat = Date.now();
            send(ws, { op: "HeartbeatAck", d: { timestamp: Date.now() } });
            break;

          case "CreateMessage": {
            if (!client.userId) break;
            const msg = engine.createMessage({
              channelId: d?.channelId as string,
              authorId: client.userId,
              authorName: client.username || "unknown",
              content: (d?.content as string) || "",
            });
            send(ws, { op: "Dispatch", t: "MESSAGE_CREATE", d: msg });
            break;
          }

          case "TypingStart": {
            const channelId = d?.channelId as string;
            send(ws, { op: "Dispatch", t: "TYPING_START", d: { userId: client.userId, username: client.username, channelId } });
            break;
          }

          case "CreateChannel": {
            if (!client.userId) break;
            const ch = engine.createChannel({ name: d?.name as string, type: (d?.type as "text" | "voice") || "text", description: (d?.description as string) || "" });
            client.channels.push(ch.id);
            engine.joinChannel(client.userId, client.username || "unknown", ch.id);
            send(ws, { op: "Dispatch", t: "CHANNEL_CREATE", d: ch });
            break;
          }

          case "JoinChannel": {
            const chId = d?.channelId as string;
            if (chId && client.userId && !client.channels.includes(chId)) {
              engine.joinChannel(client.userId, client.username || "unknown", chId);
              client.channels.push(chId);
            }
            break;
          }

          case "LeaveChannel": {
            const chId = d?.channelId as string;
            if (chId && client.userId) {
              engine.leaveChannel(client.userId, chId);
              client.channels = client.channels.filter((c) => c !== chId);
            }
            break;
          }

          case "PresenceUpdate": {
            const status = d?.status as string || "online";
            send(ws, { op: "Dispatch", t: "PRESENCE_UPDATE", d: { userId: client.userId, status } });
            break;
          }
        }
      },

      close(ws) {
        for (const [c, w] of clients) {
          if (w === ws) {
            if (c.userId) {
              send(ws, { op: "Dispatch", t: "PRESENCE_UPDATE", d: { userId: c.userId, status: "offline" } });
            }
            clients.delete(c);
            break;
          }
        }
      },
    },

    fetch(req: Request): Response | Promise<Response> {
      const url = new URL(req.url);
      if (url.pathname === "/health") {
        return new Response(JSON.stringify({ service: "nexus-team-chat", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), onlineUsers: clients.size }), { headers: { "content-type": "application/json" } });
      }
      if (url.pathname === "/api/v1/chat/status") {
        return new Response(JSON.stringify({ channels: engine.getChannels().length, messages: "active", onlineUsers: clients.size }), { headers: { "content-type": "application/json" } });
      }
      server.upgrade(req);
      return new Response("Nexus Team Chat — connect via WebSocket at /gateway", { status: 200 });
    },
  });

  console.log(`[nexus-team-chat] Listening on port ${server.port}`);
  const stopHeartbeat = startNexusTeamChatHeartbeat(baseUrl);

  return { server, close: () => {
  stopNexusHeartbeat();
  stopNexusMonitor();
  nexusClient.stop(); stopHeartbeat(); engine.close(); phantom.stop(); server.stop(); } };
}
