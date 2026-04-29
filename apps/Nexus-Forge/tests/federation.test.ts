import { describe, it, expect } from "bun:test";
import { federationEndpoints } from "../src/backend/api/federation";

describe("Federation Contract", () => {
  it("should expose a well-known service manifest", () => {
    const manifest = federationEndpoints.wellKnown();
    expect(manifest.service).toBe("nexus-forge");
    expect(manifest.endpoints.discovery).toBe("/api/cloud/discovery");
  });
});
