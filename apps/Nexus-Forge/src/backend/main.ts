import { mkdirSync } from "node:fs";
import path from "node:path";
import { env } from "bun";
import { createForge } from "./server";
import { ForgeDB } from "./storage/db";
import { RepositoryManager } from "./storage/repository";

const dbPath = env.FORGE_DB_PATH || "./data/forge.db";
const storagePath = env.FORGE_STORAGE_PATH || "./data/repos";
mkdirSync(path.dirname(dbPath), { recursive: true });
const db = new ForgeDB(dbPath);
const repos = new RepositoryManager(storagePath, db);

const port = Number.parseInt(env.PORT || "8094", 10);
// Loopback by default: public traffic reaches apps through the ecosystem
// proxy, never by binding every interface.
const host = env.HOST || "127.0.0.1";
const cloudUrl = (env.NEXUS_CLOUD_URL || "").trim();
const cloudApiKey = (env.NEXUS_CLOUD_API_KEY || "").trim();
const cloudToolId = (env.NEXUS_FORGE_TOOL_ID || "nexus-forge").trim() || "nexus-forge";
const cloudToolName = (env.NEXUS_FORGE_TOOL_NAME || "Nexus Forge").trim() || "Nexus Forge";
const cloudHeartbeatIntervalMs = Math.max(
  5000,
  Number.parseInt(env.NEXUS_CLOUD_HEARTBEAT_INTERVAL_MS || "30000", 10) || 30000,
);

function forgeUpstreamUrl(): string {
  const configured = (env.NEXUS_FORGE_PUBLIC_URL || env.PUBLIC_URL || "").trim();
  if (configured) return configured;
  const publicHost = host === "0.0.0.0" ? "localhost" : host;
  return `http://${publicHost}:${port}`;
}

function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    accept: "application/json",
    ...(cloudApiKey ? { "x-api-key": cloudApiKey } : {}),
  };
}

async function registerForgeWithCloud(): Promise<void> {
  if (!cloudUrl) return;
  const response = await fetch(`${cloudUrl.replace(/\/$/, "")}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({
      id: cloudToolId,
      name: cloudToolName,
      description: "Git hosting with signed-push policy and a verifiable ref log",
      upstreamUrl: forgeUpstreamUrl(),
      mode: "standalone",
      exposed: true,
      health: "healthy",
      capabilities: ["repository-management", "git-smart-http", "signed-push-policy", "ref-log"],
    }),
  });
  if (!response.ok) {
    throw new Error(`Nexus Cloud registration failed with status ${response.status}`);
  }
}

async function sendForgeHeartbeat(): Promise<void> {
  if (!cloudUrl) return;
  const response = await fetch(
    `${cloudUrl.replace(/\/$/, "")}/api/v1/tools/${encodeURIComponent(cloudToolId)}/heartbeat`,
    {
      method: "POST",
      headers: cloudHeaders(),
      body: JSON.stringify({ health: "healthy", upstreamUrl: forgeUpstreamUrl() }),
    },
  );
  if (!response.ok) {
    throw new Error(`Nexus Cloud heartbeat failed with status ${response.status}`);
  }
}

console.log(`🔨 Nexus Forge launching on ${host}:${port}`);
const forge = createForge({
  db,
  repos,
  ...(env.NEXUS_FORGE_PUBLIC_URL ? { publicUrl: env.NEXUS_FORGE_PUBLIC_URL } : {}),
});
const server = Bun.serve({ port, hostname: host, fetch: forge.fetch });
console.log(` Listening on http://${host}:${port}`);

let cloudHeartbeatTimer: ReturnType<typeof setInterval> | null = null;
if (cloudUrl) {
  registerForgeWithCloud()
    .then(() => {
      console.log(`[forge] Registered with Nexus Cloud as ${cloudToolId}`);
    })
    .catch((error) => {
      console.warn(`[forge] Nexus Cloud registration failed: ${(error as Error).message}`);
    });

  cloudHeartbeatTimer = setInterval(() => {
    sendForgeHeartbeat().catch((error) => {
      console.warn(`[forge] Nexus Cloud heartbeat failed: ${(error as Error).message}`);
    });
  }, cloudHeartbeatIntervalMs);
}

function stopCloudHeartbeat(): void {
  if (cloudHeartbeatTimer) {
    clearInterval(cloudHeartbeatTimer);
    cloudHeartbeatTimer = null;
  }
}

// A signal handler replaces the default "exit", so it has to exit itself;
// the earlier handlers only stopped the heartbeat and left the server
// running through every SIGTERM. In-flight requests (a push) finish first.
function shutdown(): void {
  stopCloudHeartbeat();
  server.stop().finally(() => {
    db.close();
    process.exit(0);
  });
}

process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
