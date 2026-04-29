import { entityResponse, listResponse } from "./contracts";

export function registerStrategyOKRRoutes(app: any) {
  app.get("/api/strategy/objectives", async () => listResponse([], "Objective portfolio is stubbed."));
  app.post("/api/strategy/objectives", async ({ body }: any) =>
    entityResponse({ id: "objective-stub", ...(body || {}) }, "Objective creation is stubbed.")
  );

  app.get("/api/strategy/key-results", async () => listResponse([], "Key result tracking is stubbed."));
  app.post("/api/strategy/updates", async ({ body }: any) =>
    entityResponse({ id: "okr-update-stub", ...(body || {}) }, "OKR progress updates are stubbed.")
  );

  app.get("/api/strategy/scorecards", async () =>
    entityResponse(
      {
        views: ["org", "team", "initiative"],
      },
      "Strategy scorecard views are stubbed."
    )
  );
}
