import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { createGuardianServer } from "../src/server";
import { clearDecisions } from "../src/state";

let guardianHandle: ReturnType<typeof createGuardianServer> | null = null;

beforeEach(() => {
  clearDecisions();
  process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION = "false";
  process.env.PORT = "0";
  guardianHandle = createGuardianServer();
});

afterEach(() => {
  guardianHandle?.stop();
  guardianHandle = null;
  clearDecisions();
  delete process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION;
  delete process.env.PORT;
});

describe("nexus-guardian policy integration", () => {
  test("policy deny rule forces deny decision for matching service lookup", async () => {
    const port = guardianHandle!.server.port;

    const policyRes = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/policy/public-exposure/deny`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ rules: ["blocked-app", "unsafe"] }),
    });
    expect(policyRes.status).toBe(200);

    const lookup = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/service/blocked-app-prod`);
    expect(lookup.status).toBe(200);

    const payload = await lookup.json();
    expect(payload.source).toBe("policy");
    expect(payload.decision.verdict).toBe("deny");
    expect(payload.decision.reason).toContain("blocked-app");
  });

  test("policy updates increase version", async () => {
    const port = guardianHandle!.server.port;

    const beforeRes = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/policy`);
    const before = await beforeRes.json();

    const updateRes = await fetch(`http://127.0.0.1:${port}/api/v1/guardian/policy`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ description: "Updated guardrails", enabled: true }),
    });

    expect(updateRes.status).toBe(200);
    const updated = await updateRes.json();
    expect(updated.policy.version).toBe(before.policy.version + 1);
  });
});
