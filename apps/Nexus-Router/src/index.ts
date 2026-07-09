import { createServer } from "http";
import { startRouter } from "./server";
import { logger } from "./lib/logger";
import { loadConfig } from "./lib/config";
import { registerWithCloud } from "./lib/cloud";

const PORT = parseInt(process.env.NEXUS_ROUTER_PORT || "9999", 10);
const HOSTNAME = process.env.NEXUS_ROUTER_HOST || "0.0.0.0";

async function main() {
  try {
    // Load configuration
    const configPath = process.env.CONFIG_PATH || "./config/routing-policy.yaml";
    const config = await loadConfig(configPath);
    logger.info({ configPath }, "Configuration loaded");

    // Create HTTP server
    const server = createServer((req, res) => {
      startRouter(req, res, config);
    });

    // Start listening
    server.listen(PORT, HOSTNAME, async () => {
      logger.info({ port: PORT, hostname: HOSTNAME }, "Nexus Router started");

      // Register with Nexus-Cloud
      try {
        await registerWithCloud(config);
        logger.info("Registered with Nexus-Cloud");
      } catch (err) {
        logger.warn(
          { error: err instanceof Error ? err.message : String(err) },
          "Failed to register with Nexus-Cloud (will retry)"
        );
      }

      // Start heartbeat
      setInterval(async () => {
        try {
          await registerWithCloud(config);
        } catch {
          // Silent fail, log in debug
        }
      }, (config.cloud?.heartbeat_interval_sec || 30) * 1000);
    });

    // Graceful shutdown
    process.on("SIGTERM", () => {
      logger.info("SIGTERM received, shutting down gracefully");
      server.close(() => {
        logger.info("Server closed");
        process.exit(0);
      });
    });
  } catch (err) {
    logger.error(
      { error: err instanceof Error ? err.message : String(err) },
      "Fatal error during startup"
    );
    process.exit(1);
  }
}

main();
