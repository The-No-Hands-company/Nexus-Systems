/**
 * Integration scenario: Nexus-Tunnel → Cloud Systems API registration & heartbeat,
 * and Tunnel → Guardian approval flow.
 *
 * Verifies that:
 *   1. registerNexusTunnelWithCloud sends the correct payload to Cloud /api/v1/tools
 *   2. heartbeatNexusTunnelWithCloud calls /api/v1/tools/nexus-tunnel/heartbeat
 *   3. startNexusTunnelCloudRegistrationHeartbeat fires registration on startup
 *   4. requestGuardianApprovalForExposure approves when Guardian verdict is "approve"
 *   5. requestGuardianApprovalForExposure denies when Guardian verdict is non-approve
 *   6. requestGuardianApprovalForExposure defaults to deny when Cloud is unavailable
 */

import { describe, it, afterEach } from "node:test";
import assert from "node:assert/strict";
import {
  registerNexusTunnelWithCloud,
  heartbeatNexusTunnelWithCloud,
  startNexusTunnelCloudRegistrationHeartbeat,
  requestGuardianApprovalForExposure,
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

describe("Nexus-Tunnel Cloud registration integration", () => {
  let savedFetch: typeof globalThis.fetch;
  const BASE_URL = "http://localhost:4330";
  const CLOUD_URL = "https://cloud.example.com";

  afterEach(() => {
    globalThis.fetch = savedFetch;
    delete process.env.NEXUS_CLOUD_URL;
    delete process.env.NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION;
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
      return fakeOkResponse({ tool: { id: "nexus-tunnel" } });
    }) as typeof globalThis.fetch;

    await registerNexusTunnelWithCloud(BASE_URL);

    assert.equal(calls.length, 1, "exactly one fetch call");
    assert.equal(calls[0].url, `${CLOUD_URL}/api/v1/tools`);
    assert.equal(calls[0].method, "POST");

    const payload = calls[0].body as ReturnType<typeof buildSystemsApiRegistrationPayload>;
    assert.equal(payload.id, "nexus-tunnel");
    assert.equal(payload.name, "Nexus Tunnel");
    assert.equal(payload.mode, "orchestrated");
    assert.equal(payload.upstreamUrl, BASE_URL);
    assert.ok(payload.capabilities.includes("routing"), "has routing capability");
    assert.ok(payload.capabilities.includes("tunnel"), "has tunnel capability");
    assert.ok(payload.capabilities.includes("exposure-control"), "has exposure-control capability");
    assert.ok(payload.capabilities.includes("guardian-aware"), "has guardian-aware capability");
    assert.ok((payload.metadata as Record<string, unknown>).supportsHealthAwareRouting === true);
    assert.ok((payload.metadata as Record<string, unknown>).requiresGuardianApproval === true);
  });

  it("heartbeat calls /api/v1/tools/nexus-tunnel/heartbeat with health payload", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    const calls: { url: string; body: unknown }[] = [];
    savedFetch = globalThis.fetch;
    globalThis.fetch = (async (input: RequestInfo | URL, init?: RequestInit) => {
      calls.push({ url: String(input), body: init?.body ? JSON.parse(String(init.body)) : undefined });
      return fakeOkResponse();
    }) as typeof globalThis.fetch;

    await heartbeatNexusTunnelWithCloud(BASE_URL);

    assert.equal(calls.length, 1);
    assert.equal(calls[0].url, `${CLOUD_URL}/api/v1/tools/nexus-tunnel/heartbeat`);
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

    const stop = startNexusTunnelCloudRegistrationHeartbeat(BASE_URL);
    await flushAsync(15);
    stop();

    assert.ok(
      postUrls.some((u) => u.endsWith("/api/v1/tools")),
      "registration was called on startup",
    );
  });

  it("requestGuardianApprovalForExposure approves when Guardian verdict is 'approve'", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () =>
      fakeOkResponse({ decision: { verdict: "approve", reason: "Service passed trust check" } })) as typeof globalThis.fetch;

    const result = await requestGuardianApprovalForExposure("nexus-auth", "public");
    assert.equal(result.approved, true, "approved when verdict is approve");
    assert.ok(typeof result.reason === "string" && result.reason.length > 0, "reason is a non-empty string");
  });

  it("requestGuardianApprovalForExposure denies when Guardian verdict is 'deny'", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () =>
      fakeOkResponse({ decision: { verdict: "deny", reason: "Service is unverified" } })) as typeof globalThis.fetch;

    const result = await requestGuardianApprovalForExposure("unverified-svc", "public");
    assert.equal(result.approved, false, "denied when verdict is deny");
  });

  it("requestGuardianApprovalForExposure denies by default when Cloud is unavailable", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () => { throw new Error("ECONNREFUSED"); }) as typeof globalThis.fetch;

    const result = await requestGuardianApprovalForExposure("nexus-ai", "public");
    assert.equal(result.approved, false, "defaults to deny when Cloud unreachable");
    assert.ok(result.reason?.toLowerCase().includes("unavailable"), "reason mentions unavailable");
  });

  it("requestGuardianApprovalForExposure denies when Cloud returns HTTP error", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () => fakeErrorResponse(503)) as typeof globalThis.fetch;

    const result = await requestGuardianApprovalForExposure("nexus-ai", "public");
    assert.equal(result.approved, false, "defaults to deny on HTTP error");
  });

  it("cloud integration is skipped when NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION=false", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;
    process.env.NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION = "false";

    let fetchCalled = false;
    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () => {
      fetchCalled = true;
      return fakeOkResponse();
    }) as typeof globalThis.fetch;

    const stop = startNexusTunnelCloudRegistrationHeartbeat(BASE_URL);
    await flushAsync(15);
    stop();

    assert.equal(fetchCalled, false, "fetch was not called when integration disabled");
  });
});
