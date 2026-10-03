import { describe, expect, it } from "bun:test";
import { federationEndpoints } from "../src/backend/api/federation";

describe("Federation Contract", () => {
  it("advertises only endpoints this node actually serves", () => {
    const manifest = federationEndpoints.wellKnown();
    expect(manifest.service).toBe("nexus-forge");
    expect(manifest.endpoints).toEqual({ repositories: "/api/repos" });
  });
});
