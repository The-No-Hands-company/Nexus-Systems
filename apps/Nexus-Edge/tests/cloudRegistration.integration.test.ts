/**
 * Integration scenario: Nexus-Edge → Cloud Systems API registration & heartbeat.
 *
 * Verifies that:
 *   1. registerNexusEdgeWithCloud sends the correct payload to Cloud /api/v1/tools
 *   2. heartbeatNexusEdgeWithCloud calls /api/v1/tools/nexus-edge/heartbeat
 *   3. startNexusEdgeCloudRegistrationHeartbeat fires registration on startup
 *   4. requestGuardianThreatResponse maps Cloud Guardian verdicts to Edge response actions
 *   5. requestGuardianThreatResponse is non-fatal when Cloud is unavailable
 */

import { describe, it, afterEach } from "node:test";
import assert from "node:assert/strict";
import {
  registerNexusEdgeWithCloud,
  heartbeatNexusEdgeWithCloud,
  startNexusEdgeCloudRegistrationHeartbeat,
  requestGuardianThreatResponse,
} from "../src/cloud";
import { buildSystemsApiRegistrationPayload } from "../src/contracts";

function fakeOkResponse(data: unknown = {}): Response {
  return {
    ok: true,
    status: 200,
    json: () => Promise.resolve(data),
  } as unknown as Response;
}

function fakeErrorResponse(status: number): Response {
  return {
    ok: false,
    status,
    json: () => Promise.resolve({ error: "simulated error" }),
  } as unknown as Response;
}

async function flushAsync(passes = 10): Promise<void> {
  for (let i = 0; i < passes; i++) await Promise.resolve();
}

describe("Nexus-Edge Cloud registration integration", () => {
  let savedFetch: typeof globalThis.fetch;
  const BASE_URL = "http://localhost:4340";
  const CLOUD_URL = "https://cloud.example.com";

  afterEach(() => {
    globalThis.fetch = savedFetch;
    delete process.env.NEXUS_CLOUD_URL;
    delete process.env.NEXUS_EDGE_ENABLE_CLOUD_INTEGRATION;
  });

  it("registration sends correct payload to Cloud /api/v1/tools", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    const calls: { url: string; method: string; body: unknown }[] = [];
    savedFetch = globalThis.fetch;
    globalThis.fetch = (async (input: RequestInfo | URL, init?: RequestInit) => {
      calls.push({
        url: String(input),
        method: init?.method ?? "GET",
        body: init?.body ? JSON.parse(String(init.body)) : undefined,
      });
      return fakeOkResponse({ tool: { id: "nexus-edge" } });
    }) as typeof globalThis.fetch;

    await registerNexusEdgeWithCloud(BASE_URL);

    assert.equal(calls.length, 1, "exactly one fetch call");
    assert.equal(calls[0].url, `${CLOUD_URL}/api/v1/tools`);
    assert.equal(calls[0].method, "POST");

    const payload = calls[0].body as ReturnType<typeof buildSystemsApiRegistrationPayload>;
    assert.equal(payload.id, "nexus-edge");
    assert.equal(payload.name, "Nexus Edge");
    assert.equal(payload.mode, "orchestrated");
    assert.equal(payload.upstreamUrl, BASE_URL);
    assert.ok(payload.capabilities.includes("threat-detection"), "has threat-detection capability");
    assert.ok(payload.capabilities.includes("edge-enforcement"), "has edge-enforcement capability");
    assert.ok(payload.capabilities.includes("ai-guardrails"), "has ai-guardrails capability");
    assert.ok(payload.capabilities.includes("anomaly-detection"), "has anomaly-detection capability");
    assert.ok((payload.metadata as Record<string, unknown>).supportsThreatDetection === true);
    assert.ok((payload.metadata as Record<string, unknown>).supportsAnomalyDetection === true);
  });

  it("heartbeat calls /api/v1/tools/nexus-edge/heartbeat with health payload", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    const calls: { url: string; body: unknown }[] = [];
    savedFetch = globalThis.fetch;
    globalThis.fetch = (async (input: RequestInfo | URL, init?: RequestInit) => {
      calls.push({ url: String(input), body: init?.body ? JSON.parse(String(init.body)) : undefined });
      return fakeOkResponse();
    }) as typeof globalThis.fetch;

    await heartbeatNexusEdgeWithCloud(BASE_URL);

    assert.equal(calls.length, 1);
    assert.equal(calls[0].url, `${CLOUD_URL}/api/v1/tools/nexus-edge/heartbeat`);
    assert.equal((calls[0].body as Record<string, unknown>).health, "healthy");
    assert.equal((calls[0].body as Record<string, unknown>).upstreamUrl, BASE_URL);
  });

  it("startup heartbeat loop fires registration immediately", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    const postUrls: string[] = [];
    savedFetch = globalThis.fetch;
    globalThis.fetch = (async (input: RequestInfo | URL, init?: RequestInit) => {
      if ((init?.method ?? "GET") === "POST") postUrls.push(String(input));
      return fakeOkResponse();
    }) as typeof globalThis.fetch;

    const stop = startNexusEdgeCloudRegistrationHeartbeat(BASE_URL);
    await flushAsync(15);
    stop();

    assert.ok(
      postUrls.some((u) => u.endsWith("/api/v1/tools")),
      "registration was called on startup",
    );
  });

  it("requestGuardianThreatResponse maps 'approve' verdict to log action", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () =>
      fakeOkResponse({ decision: { verdict: "approve", reason: "Trusted tool" } })) as typeof globalThis.fetch;

    const result = await requestGuardianThreatResponse("nexus-ai", "Excessive token usage");
    assert.equal(result.recommended, "log", "approve maps to log action");
  });

  it("requestGuardianThreatResponse maps 'deny' verdict to block action", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () =>
      fakeOkResponse({ decision: { verdict: "deny", reason: "Untrusted entity" } })) as typeof globalThis.fetch;

    const result = await requestGuardianThreatResponse("untrusted-agent", "Suspicious activity");
    assert.equal(result.recommended, "block", "deny maps to block action");
  });

  it("requestGuardianThreatResponse maps 'suspend' verdict to isolate action", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () =>
      fakeOkResponse({ decision: { verdict: "suspend", reason: "Quarantine recommended" } })) as typeof globalThis.fetch;

    const result = await requestGuardianThreatResponse("at-risk-agent", "Anomalous behavior");
    assert.equal(result.recommended, "isolate", "suspend maps to isolate action");
  });

  it("requestGuardianThreatResponse defaults to alert when Cloud is unavailable", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () => { throw new Error("ECONNREFUSED"); }) as typeof globalThis.fetch;

    const result = await requestGuardianThreatResponse("nexus-ai", "Threat detected");
    assert.equal(result.recommended, "alert", "defaults to alert when Cloud unreachable");
  });

  it("cloud integration is skipped when NEXUS_EDGE_ENABLE_CLOUD_INTEGRATION=false", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;
    process.env.NEXUS_EDGE_ENABLE_CLOUD_INTEGRATION = "false";

    let fetchCalled = false;
    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () => {
      fetchCalled = true;
      return fakeOkResponse();
    }) as typeof globalThis.fetch;

    const stop = startNexusEdgeCloudRegistrationHeartbeat(BASE_URL);
    await flushAsync(15);
    stop();

    assert.equal(fetchCalled, false, "fetch was not called when integration disabled");
  });
});
