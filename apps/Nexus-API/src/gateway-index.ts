import { createApiServer } from "./server";

const handle = createApiServer();

function shutdown(signal: string): void {
  console.log(`[nexus-api] received ${signal}; shutting down`);
  handle.close();
  process.exit(0);
}

process.once("SIGINT", () => shutdown("SIGINT"));
process.once("SIGTERM", () => shutdown("SIGTERM"));
