import { entityResponse, listResponse } from "./contracts";

export function registerGlobalizationReadinessRoutes(app: ForgeRouteApp) {
  app.get("/api/globalization/locales", async () =>
    listResponse([], "Globalization locale matrix is stubbed."),
  );
  app.post("/api/globalization/locales", async ({ body }) =>
    entityResponse(
      { id: "globalization-locale-stub", ...(body || {}) },
      "Locale onboarding is stubbed.",
    ),
  );

  app.get("/api/globalization/compliance", async () =>
    listResponse([], "Regional compliance readiness is stubbed."),
  );
  app.post("/api/globalization/assessments", async ({ body }) =>
    entityResponse(
      { id: "globalization-assessment-stub", payload: body || {} },
      "Globalization assessment workflow is stubbed.",
    ),
  );

  app.get("/api/globalization/checkpoints", async () =>
    listResponse([], "Global rollout checkpoints are stubbed."),
  );
}
