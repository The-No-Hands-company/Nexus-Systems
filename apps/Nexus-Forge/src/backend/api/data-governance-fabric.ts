import { entityResponse, listResponse } from "./contracts";

export function registerDataGovernanceFabricRoutes(app: any) {
  app.get("/api/data-governance/datasets", async () => listResponse([], "Dataset inventory is stubbed."));
  app.post("/api/data-governance/datasets", async ({ body }: any) =>
    entityResponse({ id: "dataset-stub", ...(body || {}) }, "Dataset registration is stubbed.")
  );

  app.get("/api/data-governance/classifications", async () => listResponse([], "Data classification catalog is stubbed."));
  app.get("/api/data-governance/lineage", async () => listResponse([], "Lineage graph API is stubbed."));

  app.get("/api/data-governance/policies", async () =>
    entityResponse(
      {
        policyFamilies: ["retention", "residency", "pii-handling", "sharing"],
      },
      "Data governance policy families are stubbed."
    )
  );
}
