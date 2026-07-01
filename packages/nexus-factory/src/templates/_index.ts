import { createNexusServer } from "@nexus/core";
import type { NexusRequestContext } from "@nexus/core";

const PORT = Number(process.env["PORT"] ?? "{{APP_PORT}}");
const SERVICE_NAME = process.env["SERVICE_NAME"] ?? "{{APP_NAME}}";

// ── Router ──────────────────────────────────────────────────────────────────

async function router(ctx: NexusRequestContext): Promise<void> {
  // TODO: add your routes here
  // Example: if (ctx.req.method === "GET" && ctx.req.url === "/api/v1/example") { ... }

  // Fallback: 404
  ctx.error(404, "not found", "NOT_FOUND");
}

// ── Server startup ──────────────────────────────────────────────────────────

const { server, close } = createNexusServer(router, {
  serviceName: SERVICE_NAME,
});

server.listen(PORT, () => {
  const a = server.address() as { port: number };
  process.stdout.write(
    JSON.stringify({
      level: "info",
      service: SERVICE_NAME,
      message: "listening",
      port: a.port,
    }) + "\n",
  );
});

async function shutdown(signal: string): Promise<void> {
  process.stdout.write(
    JSON.stringify({
      level: "info",
      service: SERVICE_NAME,
      message: "shutting down",
      signal,
    }) + "\n",
  );
  await close();
  process.exit(0);
}

process.on("SIGTERM", () => void shutdown("SIGTERM"));
process.on("SIGINT", () => void shutdown("SIGINT"));
