import { describe, it, afterEach } from "node:test";
import assert from "node:assert/strict";
import {
  reconcileNexusTunnelRoutesFromCloud,
  startNexusTunnelCloudReconciliationLoop,
} from "../src/cloud";
import { clearRoutes, getRoute, registerRoute } from "../src/state";

function fakeOkResponse(data: unknown = {}): Response {
  return {
    ok: true,
    status: 200,
    json: () => Promise.resolve(data),
  } as unknown as Response;
}

async function flushAsync(passes = 10): Promise<void> {
  for (let i = 0; i < passes; i++) await Promise.resolve();
}

describe("Nexus-Tunnel Cloud reconciliation integration", () => {
  let savedFetch: typeof globalThis.fetch;
  const CLOUD_URL = "https://cloud.example.com";

  afterEach(() => {
    globalThis.fetch = savedFetch;
    clearRoutes();
    delete process.env.NEXUS_CLOUD_URL;
    delete process.env.NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION;
    delete process.env.NEXUS_TUNNEL_CLOUD_RECONCILE_INTERVAL_MS;
  });

  it("reconciles routes from Cloud tools and disables unhealthy/degraded targets", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;
    savedFetch = globalThis.fetch;

    globalThis.fetch = (async (input: RequestInfo | URL) => {
      const url = String(input);

      if (url.endsWith("/api/v1/tools")) {
        return fakeOkResponse({
          tools: [
            {
              id: "svc-healthy",
              upstreamUrl: "http://svc-healthy:3000",
              health: "healthy",
              registrationStatus: "active",
              exposed: false,
            },
            {
              id: "svc-degraded",
              upstreamUrl: "http://svc-degraded:3001",
              health: "degraded",
              registrationStatus: "active",
              exposed: false,
            },
          ],
        });
      }

      if (url.includes("/api/v1/guardian/service/svc-healthy")) {
        return fakeOkResponse({ decision: { verdict: "approve" } });
      }
      if (url.includes("/api/v1/guardian/service/svc-degraded")) {
        return fakeOkResponse({ decision: { verdict: "approve" } });
      }

      throw new Error(`Unhandled fetch URL in test: ${url}`);
    }) as typeof globalThis.fetch;

    await reconcileNexusTunnelRoutesFromCloud();

    const healthy = getRoute("svc-healthy");
    const degraded = getRoute("svc-degraded");

    assert.ok(healthy, "healthy route exists");
    assert.equal(healthy?.enabled, true, "healthy route remains enabled");

    assert.ok(degraded, "degraded route exists");
    assert.equal(degraded?.enabled, false, "degraded route is disabled");
  });

  it("disables route when Guardian trust drops to quarantine", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;
    savedFetch = globalThis.fetch;

    globalThis.fetch = (async (input: RequestInfo | URL) => {
      const url = String(input);

      if (url.endsWith("/api/v1/tools")) {
        return fakeOkResponse({
          tools: [
            {
              id: "svc-trust-drop",
              upstreamUrl: "http://svc-trust-drop:3000",
              health: "healthy",
              registrationStatus: "active",
              exposed: true,
            },
          ],
        });
      }

      if (url.includes("/api/v1/guardian/service/svc-trust-drop")) {
        return fakeOkResponse({ decision: { verdict: "quarantine" } });
      }

      throw new Error(`Unhandled fetch URL in test: ${url}`);
    }) as typeof globalThis.fetch;

    await reconcileNexusTunnelRoutesFromCloud();

    const route = getRoute("svc-trust-drop");
    assert.ok(route, "route exists");
    assert.equal(route?.enabled, false, "route disabled due to trust drop");
    assert.equal(route?.exposureMode, "public", "exposed cloud tool maps to public exposure mode");
  });

  it("disables local routes missing from Cloud source-of-truth", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;
    savedFetch = globalThis.fetch;

    registerRoute("svc-stale", "http://svc-stale:9999", "internal");

    globalThis.fetch = (async (input: RequestInfo | URL) => {
      const url = String(input);

      if (url.endsWith("/api/v1/tools")) {
        return fakeOkResponse({ tools: [] });
      }

      throw new Error(`Unhandled fetch URL in test: ${url}`);
    }) as typeof globalThis.fetch;

    await reconcileNexusTunnelRoutesFromCloud();

    const stale = getRoute("svc-stale");
    assert.ok(stale, "stale route still exists locally");
    assert.equal(stale?.enabled, false, "stale route disabled because Cloud no longer reports tool");
  });

  it("reconciliation loop runs immediately on startup", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;
    process.env.NEXUS_TUNNEL_CLOUD_RECONCILE_INTERVAL_MS = "5000";

    let toolsCalls = 0;
    savedFetch = globalThis.fetch;

    globalThis.fetch = (async (input: RequestInfo | URL) => {
      const url = String(input);
      if (url.endsWith("/api/v1/tools")) {
        toolsCalls += 1;
        return fakeOkResponse({ tools: [] });
      }
      throw new Error(`Unhandled fetch URL in test: ${url}`);
    }) as typeof globalThis.fetch;

    const stop = startNexusTunnelCloudReconciliationLoop();
    await flushAsync(15);
    stop();

    assert.ok(toolsCalls >= 1, "reconcile loop performed immediate sync");
  });
});
