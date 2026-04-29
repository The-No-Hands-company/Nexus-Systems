import { entityResponse, listResponse } from "./contracts";

export function registerCrossProductDependencyIntelligenceRoutes(app: any) {
  app.get("/api/dependency-intel/graph", async () => listResponse([], "Cross-product dependency graph is stubbed."));
  app.get("/api/dependency-intel/hotspots", async () => listResponse([], "Dependency hotspot feed is stubbed."));

  app.post("/api/dependency-intel/contracts", async ({ body }: any) =>
    entityResponse({ id: "dependency-contract-stub", ...(body || {}) }, "Dependency contract management is stubbed.")
  );

  app.get("/api/dependency-intel/risk", async () =>
    entityResponse(
      {
        factors: ["coupling", "blast-radius", "change-frequency", "ownership"],
      },
      "Dependency risk factors are stubbed."
    )
  );
}
