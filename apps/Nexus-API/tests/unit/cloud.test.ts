import { describe, it, expect } from "vitest";
import {
  buildNexusCloudClientContract,
  buildNexusCloudDiscoveryPayload,
  buildNexusCloudRegistrationResponse,
} from "../../src/routes/cloud";

describe("Nexus Cloud integration contracts", () => {
  it("exposes a canonical discovery payload", () => {
    const payload = buildNexusCloudDiscoveryPayload();
    expect(payload.protocol).toBe("nexus-cloud/1.0");
    expect(payload.hub).toBe("Nexus Cloud");
    expect(payload.apps.some((app) => app.id === "nexus-hosting")).toBe(true);
  });

  it("exposes the client contract", () => {
    const contract = buildNexusCloudClientContract();
    expect(contract.client.endpoints.topology).toBe("/api/v1/topology");
    expect(contract.client.headers).toContain("Accept: application/json");
  });

  it("serializes registration responses", () => {
    const response = buildNexusCloudRegistrationResponse({
      appId: "nexus-hosting",
      nodeId: "node-1",
      endpoint: "https://host.example.com",
      secret: "supersecret", // pragma: allowlist secret — literal test value
      capabilities: ["topology.v1"],
    });

    expect(response.registered).toBe(true);
    expect(response.secretHint).toBe("supe...");
    expect(response.registry).toBe("/cloud/discovery");
  });
});
