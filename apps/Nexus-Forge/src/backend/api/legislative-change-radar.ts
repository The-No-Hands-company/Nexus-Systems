import { entityResponse, listResponse } from "./contracts";

export function registerLegislativeChangeRadarRoutes(app: ForgeRouteApp) {
  app.get("/api/legislative-radar/jurisdictions", async () =>
    listResponse([], "Legislative jurisdiction registry is stubbed."),
  );
  app.get("/api/legislative-radar/changes", async () =>
    listResponse([], "Legislative change feed is stubbed."),
  );

  app.post("/api/legislative-radar/impact-assessments", async ({ body }) =>
    entityResponse(
      { id: "legislative-impact-stub", ...(body || {}) },
      "Legislative impact assessment is stubbed.",
    ),
  );

  app.get("/api/legislative-radar/categories", async () =>
    entityResponse(
      {
        categories: ["privacy", "ai-governance", "cybersecurity", "labor"],
      },
      "Legislative categories are stubbed.",
    ),
  );
}
