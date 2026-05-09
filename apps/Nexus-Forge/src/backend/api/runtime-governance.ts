import { entityResponse, listResponse } from "./contracts";

export function registerRuntimeGovernanceRoutes(app: ForgeRouteApp) {
  app.get("/api/runtime/policies", async () =>
    listResponse([], "Runtime policy catalog is stubbed."),
  );
  app.post("/api/runtime/policies", async ({ body }) =>
    entityResponse(
      { id: "runtime-policy-stub", ...(body || {}) },
      "Runtime policy management is stubbed.",
    ),
  );

  app.get("/api/runtime/guardrails", async () =>
    listResponse([], "Execution guardrails are stubbed."),
  );
  app.post("/api/runtime/evaluations", async ({ body }) =>
    entityResponse(
      { id: "runtime-eval-stub", payload: body || {} },
      "Runtime evaluations are stubbed.",
    ),
  );

  app.get("/api/runtime/posture", async () =>
    entityResponse(
      {
        posture: "experimental",
      },
      "Runtime posture reporting is stubbed.",
    ),
  );
}
