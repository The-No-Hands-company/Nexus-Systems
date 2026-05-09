import { entityResponse, listResponse } from "./contracts";

export function registerIncidentLearningSystemRoutes(app: ForgeRouteApp) {
  app.get("/api/incident-learning/reports", async () =>
    listResponse([], "Incident learning reports are stubbed."),
  );
  app.post("/api/incident-learning/reports", async ({ body }) =>
    entityResponse(
      { id: "incident-learning-report-stub", ...(body || {}) },
      "Incident report capture is stubbed.",
    ),
  );

  app.get("/api/incident-learning/actions", async () =>
    listResponse([], "Corrective action tracker is stubbed."),
  );
  app.post("/api/incident-learning/blameless-retros", async ({ body }) =>
    entityResponse(
      { id: "blameless-retro-stub", payload: body || {} },
      "Blameless retrospective workflow is stubbed.",
    ),
  );

  app.get("/api/incident-learning/patterns", async () =>
    listResponse([], "Incident pattern mining is stubbed."),
  );
}
