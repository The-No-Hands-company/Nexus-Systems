import { entityResponse, listResponse } from "./contracts";

export function registerAdaptiveComplianceOrchestratorRoutes(app: ForgeRouteApp) {
  app.get("/api/adaptive-compliance/rules", async () =>
    listResponse([], "Adaptive compliance rules are stubbed."),
  );
  app.post("/api/adaptive-compliance/rules", async ({ body }) =>
    entityResponse(
      { id: "adaptive-compliance-rule-stub", ...(body || {}) },
      "Adaptive rule authoring is stubbed.",
    ),
  );

  app.get("/api/adaptive-compliance/signals", async () =>
    listResponse([], "Compliance signal ingestion is stubbed."),
  );
  app.post("/api/adaptive-compliance/actions", async ({ body }) =>
    entityResponse(
      { id: "adaptive-compliance-action-stub", payload: body || {} },
      "Adaptive remediation actioning is stubbed.",
    ),
  );

  app.get("/api/adaptive-compliance/modes", async () =>
    entityResponse(
      {
        modes: ["monitor", "advise", "enforce", "quarantine"],
      },
      "Adaptive compliance modes are stubbed.",
    ),
  );
}
