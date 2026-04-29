import { describe, expect, test, beforeEach } from "bun:test";
import { buildSystemsApiRegistrationPayload } from "../src/contracts";
import {
  recordDecision,
  getDecision,
  listDecisions,
  listAuditLog,
  getPolicy,
  updatePolicy,
  setPublicExposureDenyRules,
  evaluatePolicyDecision,
  clearDecisions,
} from "../src/state";

describe("nexus-guardian MVP contracts", () => {
  beforeEach(() => {
    clearDecisions();
  });

  test("systems-api registration payload shape is stable", () => {
    const payload = buildSystemsApiRegistrationPayload("http://localhost:4320");
    expect(payload.id).toBe("nexus-guardian");
    expect(payload.name).toBe("Nexus Guardian");
    expect(payload.mode).toBe("orchestrated");
    expect(payload.capabilities).toContain("guardian");
    expect(payload.capabilities).toContain("policy-enforcement");
    expect(payload.capabilities).toContain("audit-trail");
    expect(payload.metadata.supportsPolicyEnforcement).toBe(true);
    expect(payload.metadata.supportsAuditTrail).toBe(true);
    expect(payload.upstreamUrl).toBe("http://localhost:4320");
  });

  test("decision flow records and retrieves by scope + subjectId", () => {
    const decision = recordDecision("service", "nexus-auth", "approve", "guardian-test", "passed health check");

    expect(decision.scope).toBe("service");
    expect(decision.subjectId).toBe("nexus-auth");
    expect(decision.verdict).toBe("approve");
    expect(decision.issuedBy).toBe("guardian-test");
    expect(decision.reason).toBe("passed health check");
    expect(decision.id).toMatch(/^dec-/);
    expect(decision.issuedAt).toBeTruthy();

    const retrieved = getDecision("service", "nexus-auth");
    expect(retrieved).toBeDefined();
    expect(retrieved?.verdict).toBe("approve");
  });

  test("decision overwrite replaces latest verdict for same subject", () => {
    recordDecision("user", "alice", "approve");
    recordDecision("user", "alice", "suspend", "guardian", "suspicious activity");

    const current = getDecision("user", "alice");
    expect(current?.verdict).toBe("suspend");

    const all = listDecisions();
    expect(all.filter((d) => d.subjectId === "alice").length).toBe(1);
  });

  test("audit log emits entry for each decision recorded", () => {
    recordDecision("service", "nexus-vault", "approve");
    recordDecision("agent", "ai-agent-1", "quarantine", "guardian", "anomaly detected");

    const audit = listAuditLog();
    expect(audit.length).toBe(2);
    expect(audit[0].scope).toBe("service");
    expect(audit[0].verdict).toBe("approve");
    expect(audit[1].subjectId).toBe("ai-agent-1");
    expect(audit[1].verdict).toBe("quarantine");
  });

  test("listDecisions returns all active decisions", () => {
    recordDecision("service", "svc-a", "approve");
    recordDecision("service", "svc-b", "deny");
    recordDecision("network", "net-1", "suspend");

    const all = listDecisions();
    expect(all.length).toBe(3);
    expect(all.map((d) => d.subjectId).sort()).toEqual(["net-1", "svc-a", "svc-b"]);
  });

  test("policy version increments when policy fields are updated", () => {
    const before = getPolicy();
    const updated = updatePolicy({ description: "Hardened policy", enabled: true });

    expect(updated.version).toBe(before.version + 1);
    expect(updated.description).toBe("Hardened policy");
  });

  test("public exposure deny rules produce effective deny decision for matching service", () => {
    setPublicExposureDenyRules(["unsafe", "blocked-service"]);

    const matched = evaluatePolicyDecision("service", "unsafe-gateway");
    expect(matched).toBeDefined();
    expect(matched?.verdict).toBe("deny");
    expect(matched?.issuedBy).toBe("guardian-policy");

    const nonMatched = evaluatePolicyDecision("service", "trusted-service");
    expect(nonMatched).toBeUndefined();
  });
});
