import { describe, expect, test } from "bun:test";
import {
  exampleComplianceSummary,
  exampleRouteEntry,
  exampleStatusResponse,
  exampleToolHeartbeat,
  exampleToolRegistration,
} from "../src/examples";
import { systemsApiV1Endpoints } from "../src/contracts";

describe("nexus systems api artifact contracts", () => {
  test("pins critical v1 endpoints", () => {
    expect(systemsApiV1Endpoints.registerTool).toBe("/api/v1/tools");
    expect(systemsApiV1Endpoints.status).toBe("/api/v1/status");
    expect(systemsApiV1Endpoints.phantomComplianceSummary).toBe("/api/v1/compliance/phantom/summary");
  });

  test("ships stable example payload set", () => {
    expect(exampleToolRegistration.id).toBe("nexus-auth");
    expect(exampleToolHeartbeat.status).toBe("healthy");
    expect(exampleStatusResponse.version).toBe("v1");
    expect(exampleRouteEntry.securityTag).toBe("transitional");
    expect(exampleComplianceSummary.status).toBe("failing");
  });
});