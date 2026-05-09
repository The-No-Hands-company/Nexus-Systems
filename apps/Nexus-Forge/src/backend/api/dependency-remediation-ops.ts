import { entityResponse, listResponse } from "./contracts";

export function registerDependencyRemediationOpsRoutes(app: ForgeRouteApp) {
  app.get("/api/dependency-remediation/findings", async () =>
    listResponse([], "Dependency findings are stubbed."),
  );
  app.post("/api/dependency-remediation/findings", async ({ body }) =>
    entityResponse(
      { id: "dependency-finding-stub", ...(body || {}) },
      "Dependency finding intake is stubbed.",
    ),
  );

  app.get("/api/dependency-remediation/plans", async () =>
    listResponse([], "Dependency remediation plans are stubbed."),
  );
  app.post("/api/dependency-remediation/actions", async ({ body }) =>
    entityResponse(
      { id: "dependency-remediation-action-stub", payload: body || {} },
      "Remediation action workflow is stubbed.",
    ),
  );

  app.get("/api/dependency-remediation/risk-bands", async () =>
    entityResponse(
      {
        bands: ["critical", "high", "medium", "low"],
      },
      "Dependency remediation risk bands are stubbed.",
    ),
  );
}
