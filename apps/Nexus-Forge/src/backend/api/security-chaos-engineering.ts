import { entityResponse, listResponse } from "./contracts";

export function registerSecurityChaosEngineeringRoutes(app: ForgeRouteApp) {
  app.get("/api/security-chaos/experiments", async () =>
    listResponse([], "Security chaos experiments are stubbed."),
  );
  app.post("/api/security-chaos/experiments", async ({ body }) =>
    entityResponse(
      { id: "security-chaos-experiment-stub", ...(body || {}) },
      "Experiment creation is stubbed.",
    ),
  );

  app.get("/api/security-chaos/findings", async () =>
    listResponse([], "Security chaos findings are stubbed."),
  );
  app.post("/api/security-chaos/remediations", async ({ body }) =>
    entityResponse(
      { id: "security-chaos-remediation-stub", payload: body || {} },
      "Remediation workflow is stubbed.",
    ),
  );

  app.get("/api/security-chaos/scopes", async () =>
    entityResponse(
      {
        scopes: ["identity", "network", "supply-chain", "runtime"],
      },
      "Security chaos scopes are stubbed.",
    ),
  );
}
