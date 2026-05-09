import { entityResponse, listResponse } from "./contracts";

export function registerDeveloperExperienceRoutes(app: ForgeRouteApp) {
  app.get("/api/devex/onboarding", async () =>
    entityResponse(
      {
        journeys: ["new-repo", "first-pr", "first-release"],
      },
      "Developer onboarding journeys are stubbed.",
    ),
  );

  app.get("/api/devex/templates", async () =>
    listResponse([], "Starter template registry is stubbed."),
  );
  app.post("/api/devex/templates", async ({ body }) =>
    entityResponse(
      { id: "starter-template-stub", ...(body || {}) },
      "Starter template creation is stubbed.",
    ),
  );

  app.get("/api/devex/scorecards", async () =>
    listResponse([], "Repository experience scorecards are stubbed."),
  );
  app.get("/api/devex/recommendations", async () =>
    listResponse([], "Developer guidance recommendations are stubbed."),
  );
}
