import { describe, expect, test, beforeEach } from "bun:test";
import { buildSystemsApiRegistrationPayload } from "../src/contracts";
import {
  registerRoute,
  getRoute,
  listRoutes,
  recordExposure,
  getExposure,
  listExposures,
  recordDecision,
  listDecisions,
  getPolicy,
  clearRoutes,
} from "../src/state";

describe("nexus-tunnel MVP contracts", () => {
  beforeEach(() => {
    clearRoutes();
  });

  test("systems-api registration payload shape is stable", () => {
    const payload = buildSystemsApiRegistrationPayload("http://localhost:4330");
    expect(payload.id).toBe("nexus-tunnel");
    expect(payload.name).toBe("Nexus Tunnel");
    expect(payload.mode).toBe("orchestrated");
    expect(payload.capabilities).toContain("routing");
    expect(payload.capabilities).toContain("tunnel");
    expect(payload.capabilities).toContain("exposure-control");
    expect(payload.capabilities).toContain("guardian-aware");
    expect(payload.metadata.supportsHealthAwareRouting).toBe(true);
    expect(payload.metadata.requiresGuardianApproval).toBe(true);
    expect(payload.upstreamUrl).toBe("http://localhost:4330");
  });

  test("route registration and retrieval flow", () => {
    const route = registerRoute("nexus-auth", "http://localhost:4310", "internal");

    expect(route.toolId).toBe("nexus-auth");
    expect(route.targetUrl).toBe("http://localhost:4310");
    expect(route.exposureMode).toBe("internal");
    expect(route.enabled).toBe(true);
    expect(route.routeId).toMatch(/^route-/);

    const retrieved = getRoute("nexus-auth");
    expect(retrieved).toBeDefined();
    expect(retrieved?.routeId).toBe(route.routeId);
  });

  test("exposure decision records and retrieves approval status", () => {
    const exposure = recordExposure("nexus-cloud", "public", true, "passed guardian review");

    expect(exposure.toolId).toBe("nexus-cloud");
    expect(exposure.exposureMode).toBe("public");
    expect(exposure.guardianApproved).toBe(true);
    expect(exposure.approvalReason).toBe("passed guardian review");
    expect(exposure.requestedAt).toBeTruthy();
    expect(exposure.approvedAt).toBeTruthy();

    const retrieved = getExposure("nexus-cloud");
    expect(retrieved?.guardianApproved).toBe(true);
  });

  test("denial exposure decision records rejection reason", () => {
    const exposure = recordExposure("untrusted-service", "public", false, "Guardian denied: suspicious activity");

    expect(exposure.guardianApproved).toBe(false);
    expect(exposure.denialReason).toBe("Guardian denied: suspicious activity");
    expect(exposure.approvedAt).toBeUndefined();
  });

  test("decision flow records routing choices", () => {
    recordDecision("nexus-vault", "protected", true, "tunnel", "Health check passed");
    recordDecision("nexus-ai", "internal", true, "tunnel", "Standard internal routing");

    const all = listDecisions();
    expect(all.length).toBe(2);
    expect(all[0].approved).toBe(true);
    expect(all[1].approved).toBe(true);
  });

  test("listRoutes and listExposures return all registered items", () => {
    registerRoute("svc-a", "http://svc-a:3000", "public");
    registerRoute("svc-b", "http://svc-b:3001", "internal");
    recordExposure("svc-a", "public", true);
    recordExposure("svc-b", "internal", true);

    const routes = listRoutes();
    const exposures = listExposures();

    expect(routes.length).toBe(2);
    expect(exposures.length).toBe(2);
    expect(routes.map((r) => r.toolId).sort()).toEqual(["svc-a", "svc-b"]);
  });

  test("policy enforces guardian approval requirement for public exposure", () => {
    const policy = getPolicy();
    expect(policy.requireGuardianApproval).toBe(true);
    expect(policy.maxPublicExposures).toBe(10);
    expect(policy.healthCheckIntervalMs).toBe(30000);
  });
});
