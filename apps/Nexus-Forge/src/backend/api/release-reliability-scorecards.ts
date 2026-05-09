import { entityResponse, listResponse } from "./contracts";

export function registerReleaseReliabilityScorecardsRoutes(app: ForgeRouteApp) {
  app.get("/api/release-reliability/scorecards", async () =>
    listResponse([], "Release reliability scorecards are stubbed."),
  );
  app.get("/api/release-reliability/sli", async () =>
    listResponse([], "Release reliability SLI stream is stubbed."),
  );

  app.post("/api/release-reliability/budgets", async ({ body }) =>
    entityResponse(
      { id: "release-budget-stub", ...(body || {}) },
      "Release reliability budget management is stubbed.",
    ),
  );

  app.get("/api/release-reliability/dimensions", async () =>
    entityResponse(
      {
        dimensions: ["rollback-rate", "change-failure", "lead-time", "availability"],
      },
      "Release reliability dimensions are stubbed.",
    ),
  );
}
