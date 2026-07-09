import axios from "axios";
import { logger } from "./logger";
import { Config } from "./config";

const PHANTOM_DID = `did:phantom:${crypto.randomUUID().replace(/-/g, "").slice(0, 32)}`;
const ROUTER_ID = `nexus-router-${Math.random().toString(36).slice(2, 8)}`;

export async function registerWithCloud(config: Config): Promise<void> {
  try {
    const cloudUrl = config.cloud?.url || "http://localhost:8787";
    const apiKey = config.cloud?.apiKey || "change-me";
    const port = config.router?.port || 9999;

    const toolPayload = {
      id: ROUTER_ID,
      name: "nexus-router",
      type: "router",
      description: "Central orchestration engine for Nexus Systems ecosystem",
      baseUrl: `http://localhost:${port}`,
      capabilities: [
        "auth",
        "routing",
        "ai-provider-selection",
        "telemetry",
        "federation-aware",
      ],
      phantom_did: PHANTOM_DID,
      version: "0.1.0",
    };

    const response = await axios.post(`${cloudUrl}/api/v1/tools`, toolPayload, {
      headers: {
        Authorization: `Bearer ${apiKey}`,
        "Content-Type": "application/json",
      },
      timeout: 5000,
    });

    logger.info(
      { tool_id: ROUTER_ID, cloud_url: cloudUrl },
      "Registered with Nexus-Cloud"
    );

    // Start heartbeat
    setInterval(async () => {
      try {
        await axios.post(`${cloudUrl}/api/v1/tools/${ROUTER_ID}/heartbeat`, {}, {
          headers: {
            Authorization: `Bearer ${apiKey}`,
          },
          timeout: 5000,
        });
      } catch (err) {
        logger.debug(
          {
            error: err instanceof Error ? err.message : String(err),
          },
          "Heartbeat failed (will retry)"
        );
      }
    }, (config.cloud?.heartbeat_interval_sec || 30) * 1000);
  } catch (err) {
    logger.error(
      { error: err instanceof Error ? err.message : String(err) },
      "Failed to register with Nexus-Cloud"
    );
    throw err;
  }
}
