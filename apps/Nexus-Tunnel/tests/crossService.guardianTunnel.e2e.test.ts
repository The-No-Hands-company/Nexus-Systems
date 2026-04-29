import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { rmSync } from "node:fs";
import { join } from "node:path";
import { createGuardianServer } from "../../Nexus-Guardian/src/server";
import { createTunnelServer } from "../src/server";

type GuardianHandle = ReturnType<typeof createGuardianServer>;
type TunnelHandle = ReturnType<typeof createTunnelServer>;

let guardian: GuardianHandle | null = null;
let tunnel: TunnelHandle | null = null;
let upstream: ReturnType<typeof Bun.serve> | null = null;
let guardianStatePath = "";
let tunnelStatePath = "";

function uniqueStatePath(prefix: string): string {
  return join(process.cwd(), "tests", `${prefix}-${Date.now()}-${Math.random().toString(16).slice(2)}.json`);
}

async function createPublicRoute(toolId: string, targetUrl: string): Promise<void> {
  const res = await fetch(`http://127.0.0.1:${tunnel!.server.port}/api/v1/tunnel/routes`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ toolId, targetUrl, exposureMode: "public" }),
  });
  expect(res.status).toBe(201);
}

async function setGuardianDecision(toolId: string, verdict: "approve" | "deny"): Promise<void> {
  const res = await fetch(
    `http://127.0.0.1:${guardian!.server.port}/api/v1/guardian/service/${encodeURIComponent(toolId)}/${verdict}`,
    {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ reason: `e2e-${verdict}`, issuedBy: "e2e-test" }),
    },
  );
  expect(res.status).toBe(201);
}

beforeEach(() => {
  guardianStatePath = uniqueStatePath("tmp-guardian-state-e2e");
  tunnelStatePath = uniqueStatePath("tmp-tunnel-state-e2e");

  process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION = "false";
  process.env.NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION = "false";
  process.env.NEXUS_GUARDIAN_STATE_PATH = guardianStatePath;
  process.env.NEXUS_TUNNEL_STATE_PATH = tunnelStatePath;

  process.env.PORT = "0";
  guardian = createGuardianServer();

  const guardianUrl = `http://127.0.0.1:${guardian.server.port}`;
  process.env.NEXUS_CLOUD_URL = guardianUrl;
  process.env.PORT = "0";
  tunnel = createTunnelServer();

  upstream = Bun.serve({
    port: 0,
    fetch: async (request) => {
      const url = new URL(request.url);
      return new Response(
        JSON.stringify(
          {
            ok: true,
            from: "upstream",
            method: request.method,
            path: `${url.pathname}${url.search}`,
          },
          null,
          2,
        ),
        { headers: { "content-type": "application/json" } },
      );
    },
  });
});

afterEach(() => {
  tunnel?.stop();
  guardian?.stop();
  upstream?.stop();
  tunnel = null;
  guardian = null;
  upstream = null;

  delete process.env.PORT;
  delete process.env.NEXUS_CLOUD_URL;
  delete process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION;
  delete process.env.NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION;
  delete process.env.NEXUS_GUARDIAN_STATE_PATH;
  delete process.env.NEXUS_TUNNEL_STATE_PATH;

  rmSync(guardianStatePath, { force: true });
  rmSync(tunnelStatePath, { force: true });
});

describe("cross-service e2e: register -> expose -> guardian allow/deny -> route enabled/blocked", () => {
  test("guardian approve enables public exposure and proxy path", async () => {
    const toolId = "svc-e2e-allow";
    const targetUrl = `http://127.0.0.1:${upstream!.port}`;

    await createPublicRoute(toolId, targetUrl);
    await setGuardianDecision(toolId, "approve");

    const exposeRes = await fetch(`http://127.0.0.1:${tunnel!.server.port}/api/v1/tunnel/decide/${toolId}`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ exposureMode: "public" }),
    });

    expect(exposeRes.status).toBe(200);
    const exposePayload = await exposeRes.json();
    expect(exposePayload.status).toBe("approved");
    expect(exposePayload.exposure.guardianApproved).toBe(true);

    const proxied = await fetch(`http://127.0.0.1:${tunnel!.server.port}/t/${toolId}/v1/ping?e2e=1`);
    expect(proxied.status).toBe(200);

    const proxiedPayload = await proxied.json();
    expect(proxiedPayload.from).toBe("upstream");
    expect(proxiedPayload.path).toBe("/v1/ping?e2e=1");
  });

  test("guardian deny blocks public exposure and proxy path", async () => {
    const toolId = "svc-e2e-deny";
    const targetUrl = `http://127.0.0.1:${upstream!.port}`;

    await createPublicRoute(toolId, targetUrl);
    await setGuardianDecision(toolId, "deny");

    const exposeRes = await fetch(`http://127.0.0.1:${tunnel!.server.port}/api/v1/tunnel/decide/${toolId}`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ exposureMode: "public" }),
    });

    expect(exposeRes.status).toBe(403);
    const exposePayload = await exposeRes.json();
    expect(exposePayload.status).toBe("denied");
    expect(exposePayload.exposure.guardianApproved).toBe(false);

    const proxied = await fetch(`http://127.0.0.1:${tunnel!.server.port}/t/${toolId}/v1/ping`);
    expect(proxied.status).toBe(403);

    const payload = await proxied.json();
    expect(payload.error).toContain("Guardian approval");
  });
});
