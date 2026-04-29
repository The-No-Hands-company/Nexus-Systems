import { Elysia } from "elysia";
import { cors } from "@elysiajs/cors";
import { env } from "bun";
import { registerRoutes } from "./api/routes";
import { registerFederationRoutes } from "./api/federation";
import { ForgeDB } from "./storage/db";
import { RepositoryManager } from "./storage/repository";

const dbPath = env.FORGE_DB_PATH || "./data/forge-meta.db";
const storagePath = env.FORGE_STORAGE_PATH || "./data/repos";
const db = new ForgeDB(dbPath);
const repoManager = new RepositoryManager(storagePath, db);

const app = new Elysia()
  .use(cors())
  .get("/health", () => ({ status: "ok", timestamp: new Date().toISOString() }));

registerRoutes(app, repoManager);
registerFederationRoutes(app);

const port = parseInt(env.PORT || "8090", 10);
const host = env.HOST || "0.0.0.0";
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
      description: "Federated code forge for repositories, issues, and collaboration",
      upstreamUrl: forgeUpstreamUrl(),
      mode: "standalone",
      exposed: true,
      health: "healthy",
      capabilities: [
        "repository-management",
        "federation-discovery",
        "pull-requests",
        "issues",
        "multi-vcs",
      ],
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
app.listen({ port, hostname: host });
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

process.on("SIGINT", () => stopCloudHeartbeat());
process.on("SIGTERM", () => stopCloudHeartbeat());
