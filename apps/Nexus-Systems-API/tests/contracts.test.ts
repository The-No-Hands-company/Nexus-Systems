import { describe, expect, test } from "bun:test";
import { batchServiceRegistry, systemsApiV1Endpoints } from "../src/contracts";
import {
  batchServiceRegistrations,
  exampleBatchHeartbeat,
  exampleComplianceSummary,
  exampleRouteEntry,
  exampleStatusResponse,
  exampleToolHeartbeat,
  exampleToolRegistration,
} from "../src/examples";

describe("nexus systems api artifact contracts", () => {
  test("pins critical v1 endpoints", () => {
    expect(systemsApiV1Endpoints.registerTool).toBe("/api/v1/tools");
    expect(systemsApiV1Endpoints.status).toBe("/api/v1/status");
    expect(systemsApiV1Endpoints.heartbeat).toBe("/api/v1/tools/:toolId/heartbeat");
    expect(systemsApiV1Endpoints.phantomComplianceSummary).toBe(
      "/api/v1/compliance/phantom/summary",
    );
  });

  test("ships stable example payload set", () => {
    expect(exampleToolRegistration.id).toBe("nexus-auth");
    expect(exampleToolHeartbeat.status).toBe("healthy");
    expect(exampleStatusResponse.version).toBe("v1");
    expect(exampleRouteEntry.securityTag).toBe("transitional");
    expect(exampleComplianceSummary.status).toBe("failing");
  });

  test("pins all 9 batch service registrations", () => {
    const batchIds = Object.keys(batchServiceRegistry);
    expect(batchIds).toHaveLength(9);
    for (const id of batchIds) {
      const reg = batchServiceRegistrations[id];
      expect(reg).toBeTruthy();
      expect(reg.id).toBe(id);
      expect(reg.role).toBe(batchServiceRegistry[id as keyof typeof batchServiceRegistry].role);
      expect(reg.upstreamUrl).toContain(
        String(batchServiceRegistry[id as keyof typeof batchServiceRegistry].port),
      );
    }
  });

  test("batch heartbeat example is well-formed", () => {
    expect(exampleBatchHeartbeat.toolId).toBe("nexus-monitor");
    expect(exampleBatchHeartbeat.status).toBe("healthy");
    expect(exampleBatchHeartbeat.upstreamUrl).toContain("3030");
  });
});
