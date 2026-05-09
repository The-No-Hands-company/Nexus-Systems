import { entityResponse, listResponse } from "./contracts";

export function registerArtifactTrustChainRoutes(app: ForgeRouteApp) {
  app.get("/api/artifact-trust/artifacts", async () =>
    listResponse([], "Artifact trust registry is stubbed."),
  );
  app.post("/api/artifact-trust/artifacts", async ({ body }) =>
    entityResponse(
      { id: "artifact-trust-artifact-stub", ...(body || {}) },
      "Artifact trust enrollment is stubbed.",
    ),
  );

  app.get("/api/artifact-trust/attestations", async () =>
    listResponse([], "Artifact attestation ledger is stubbed."),
  );
  app.post("/api/artifact-trust/verifications", async ({ body }) =>
    entityResponse(
      { id: "artifact-trust-verification-stub", payload: body || {} },
      "Artifact verification workflow is stubbed.",
    ),
  );

  app.get("/api/artifact-trust/provenance", async () =>
    listResponse([], "Artifact provenance graph is stubbed."),
  );
}
