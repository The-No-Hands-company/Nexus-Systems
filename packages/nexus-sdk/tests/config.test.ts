import { afterEach, describe, expect, test } from "bun:test";
import { createConfig } from "../src/config";

/**
 * Where a service sends its registration and heartbeats.
 *
 * NEXUS_CLOUD_URL is Cloud's *public* address (it hands it to federated peers
 * for discovery). A heartbeat sent there goes out through the internet and back
 * into the proxy, whose sign-in gate refuses it: Cloud marked itself offline
 * that way from 2026-09-29. NEXUS_CLOUD_INTERNAL_URL names the local address
 * and wins when set.
 */

const options = { id: "nexus-test", name: "Test", description: "", port: 1234, capabilities: [] };
const saved = { internal: process.env.NEXUS_CLOUD_INTERNAL_URL, url: process.env.NEXUS_CLOUD_URL };

afterEach(() => {
  for (const [key, value] of [["NEXUS_CLOUD_INTERNAL_URL", saved.internal], ["NEXUS_CLOUD_URL", saved.url]] as const) {
    if (value === undefined) delete process.env[key];
    else process.env[key] = value;
  }
});

describe("createConfig's cloud address", () => {
  test("when an internal address is set, should send heartbeats there, not to the public one", () => {
    process.env.NEXUS_CLOUD_URL = "https://cloud.tnhc.dev";
    process.env.NEXUS_CLOUD_INTERNAL_URL = "http://127.0.0.1:8787/";
    expect(createConfig(options).cloudUrl).toBe("http://127.0.0.1:8787");
  });

  test("when only NEXUS_CLOUD_URL is set, should keep using it as before", () => {
    delete process.env.NEXUS_CLOUD_INTERNAL_URL;
    process.env.NEXUS_CLOUD_URL = "http://localhost:8787";
    expect(createConfig(options).cloudUrl).toBe("http://localhost:8787");
  });

  test("when neither is set, should default to the local Cloud", () => {
    delete process.env.NEXUS_CLOUD_INTERNAL_URL;
    delete process.env.NEXUS_CLOUD_URL;
    expect(createConfig(options).cloudUrl).toBe("http://localhost:8787");
  });
});

describe("heartbeat failures", () => {
  test("should be logged once when they start and once when they recover, not on every beat", async () => {
    const { NexusClient } = await import("../src/client");
    let status = 401;
    const cloud = Bun.serve({ port: 0, hostname: "127.0.0.1", fetch: () => new Response(null, { status }) });
    process.env.NEXUS_CLOUD_INTERNAL_URL = `http://127.0.0.1:${cloud.port}`;
    const warnings: string[] = [];
    const original = console.warn;
    console.warn = (msg: string) => { warnings.push(msg); };
    try {
      const client = new NexusClient(createConfig(options));
      await client.heartbeatWithCloud();
      await client.heartbeatWithCloud();
      status = 200;
      await client.heartbeatWithCloud();
      await client.heartbeatWithCloud();
      expect(warnings).toHaveLength(2);
      expect(warnings[0]).toContain("Cloud heartbeat failing (HTTP 401)");
      expect(warnings[1]).toContain("recovered");
    } finally {
      console.warn = original;
      cloud.stop(true);
    }
  });
});
