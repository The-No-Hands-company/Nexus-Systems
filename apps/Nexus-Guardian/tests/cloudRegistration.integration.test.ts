/**
 * Integration scenario: Nexus-Guardian → Cloud Systems API registration & heartbeat.
 *
 * Verifies that:
 *   1. registerNexusGuardianWithCloud sends the correct payload to Cloud /api/v1/tools
 *   2. heartbeatNexusGuardianWithCloud calls /api/v1/tools/nexus-guardian/heartbeat
 *   3. startNexusGuardianCloudRegistrationHeartbeat fires registration on startup
 *   4. When Cloud is unavailable, registration failure is non-fatal (does not throw)
 *
 * Uses plain fake-response objects so .json() resolves via pure Promise microtasks.
 */

import { describe, it, afterEach } from "node:test";
import assert from "node:assert/strict";
import {
  registerNexusGuardianWithCloud,
  heartbeatNexusGuardianWithCloud,
  startNexusGuardianCloudRegistrationHeartbeat,
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

describe("Nexus-Guardian Cloud registration integration", () => {
  let savedFetch: typeof globalThis.fetch;
  const BASE_URL = "http://localhost:4320";
  const CLOUD_URL = "https://cloud.example.com";

  afterEach(() => {
    globalThis.fetch = savedFetch;
    delete process.env.NEXUS_CLOUD_URL;
    delete process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION;
  });

  it("registration sends correct payload to Cloud /api/v1/tools", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    const calls: { url: string; method: string; body: unknown }[] = [];
    savedFetch = globalThis.fetch;
    globalThis.fetch = (async (input: RequestInfo | URL, init?: RequestInit) => {
      const url = String(input);
      const body = init?.body ? JSON.parse(String(init.body)) : undefined;
      calls.push({ url, method: init?.method ?? "GET", body });
      return fakeOkResponse({ tool: { id: "nexus-guardian" } });
    }) as typeof globalThis.fetch;

    await registerNexusGuardianWithCloud(BASE_URL);

    assert.equal(calls.length, 1, "exactly one fetch call");
    assert.equal(calls[0].url, `${CLOUD_URL}/api/v1/tools`);
    assert.equal(calls[0].method, "POST");

    const payload = calls[0].body as ReturnType<typeof buildSystemsApiRegistrationPayload>;
    assert.equal(payload.id, "nexus-guardian");
    assert.equal(payload.name, "Nexus Guardian");
    assert.equal(payload.mode, "orchestrated");
    assert.equal(payload.upstreamUrl, BASE_URL);
    assert.ok(Array.isArray(payload.capabilities), "capabilities is an array");
    assert.ok(payload.capabilities.includes("guardian"), "has guardian capability");
    assert.ok(payload.capabilities.includes("policy-enforcement"), "has policy-enforcement capability");
    assert.ok(payload.capabilities.includes("audit-trail"), "has audit-trail capability");
    assert.ok((payload.metadata as Record<string, unknown>).supportsPolicyEnforcement === true);
    assert.ok((payload.metadata as Record<string, unknown>).supportsAuditTrail === true);
  });

  it("heartbeat calls /api/v1/tools/nexus-guardian/heartbeat with health payload", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    const calls: { url: string; method: string; body: unknown }[] = [];
    savedFetch = globalThis.fetch;
    globalThis.fetch = (async (input: RequestInfo | URL, init?: RequestInit) => {
      calls.push({
        url: String(input),
        method: init?.method ?? "GET",
        body: init?.body ? JSON.parse(String(init.body)) : undefined,
      });
      return fakeOkResponse();
    }) as typeof globalThis.fetch;

    await heartbeatNexusGuardianWithCloud(BASE_URL);

    assert.equal(calls.length, 1, "exactly one heartbeat call");
    assert.equal(calls[0].url, `${CLOUD_URL}/api/v1/tools/nexus-guardian/heartbeat`);
    assert.equal(calls[0].method, "POST");
    assert.equal((calls[0].body as Record<string, unknown>).health, "healthy");
    assert.equal((calls[0].body as Record<string, unknown>).upstreamUrl, BASE_URL);
  });

  it("startup heartbeat loop fires registration immediately", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    const registrationUrls: string[] = [];
    savedFetch = globalThis.fetch;
    globalThis.fetch = (async (input: RequestInfo | URL, init?: RequestInit) => {
      const url = String(input);
      if ((init?.method ?? "GET") === "POST") registrationUrls.push(url);
      return fakeOkResponse();
    }) as typeof globalThis.fetch;

    const stop = startNexusGuardianCloudRegistrationHeartbeat(BASE_URL);
    await flushAsync(15);
    stop();

    assert.ok(
      registrationUrls.some((u) => u.includes("/api/v1/tools") && !u.includes("/heartbeat")),
      "registration was called on startup",
    );
  });

  it("startup registration is non-fatal when Cloud returns an error", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;

    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () => fakeErrorResponse(503)) as typeof globalThis.fetch;

    // Should not throw — errors are swallowed with console.warn
    const stop = startNexusGuardianCloudRegistrationHeartbeat(BASE_URL);
    await flushAsync(15);
    stop();
    // If we reach here, the error was non-fatal
    assert.ok(true, "error was non-fatal");
  });

  it("cloud integration is skipped when NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION=false", async () => {
    process.env.NEXUS_CLOUD_URL = CLOUD_URL;
    process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION = "false";

    let fetchCalled = false;
    savedFetch = globalThis.fetch;
    globalThis.fetch = (async () => {
      fetchCalled = true;
      return fakeOkResponse();
    }) as typeof globalThis.fetch;

    const stop = startNexusGuardianCloudRegistrationHeartbeat(BASE_URL);
    await flushAsync(15);
    stop();

    assert.equal(fetchCalled, false, "fetch was not called when integration disabled");
  });
});
