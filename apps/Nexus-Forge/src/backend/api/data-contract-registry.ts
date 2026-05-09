import { entityResponse, listResponse } from "./contracts";

export function registerDataContractRegistryRoutes(app: ForgeRouteApp) {
  app.get("/api/data-contracts/contracts", async () =>
    listResponse([], "Data contract catalog is stubbed."),
  );
  app.post("/api/data-contracts/contracts", async ({ body }) =>
    entityResponse(
      { id: "data-contract-stub", ...(body || {}) },
      "Data contract creation is stubbed.",
    ),
  );

  app.get("/api/data-contracts/versions", async () =>
    listResponse([], "Data contract version graph is stubbed."),
  );
  app.post("/api/data-contracts/verifications", async ({ body }) =>
    entityResponse(
      { id: "data-contract-verification-stub", payload: body || {} },
      "Data contract verification is stubbed.",
    ),
  );

  app.get("/api/data-contracts/compliance", async () =>
    listResponse([], "Data contract compliance reporting is stubbed."),
  );
}
