import { entityResponse, listResponse } from "./contracts";

export function registerSupplyChainRoutes(app: ForgeRouteApp) {
  app.get("/api/supply-chain/sbom", async () => listResponse([], "SBOM generation is stubbed."));
  app.post("/api/supply-chain/sbom", async ({ body }) =>
    entityResponse({ id: "sbom-stub", ...(body || {}) }, "SBOM creation is stubbed."),
  );

  app.get("/api/supply-chain/attestations", async () =>
    listResponse([], "Artifact attestations are stubbed."),
  );
  app.post("/api/supply-chain/attestations", async ({ body }) =>
    entityResponse({ id: "attestation-stub", ...(body || {}) }, "Attestation signing is stubbed."),
  );

  app.get("/api/supply-chain/provenance", async () =>
    entityResponse({ chains: [], signatures: [] }, "Build provenance graph is stubbed."),
  );

  app.get("/api/supply-chain/policies", async () =>
    listResponse([], "Supply-chain policy engine is stubbed."),
  );
}
