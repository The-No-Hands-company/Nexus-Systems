import { entityResponse, listResponse } from "./contracts";

export function registerAutonomousBacklogTriageRoutes(app: ForgeRouteApp) {
  app.get("/api/backlog/intake", async () => listResponse([], "Backlog intake queue is stubbed."));
  app.get("/api/backlog/classifications", async () =>
    listResponse([], "Backlog classification feed is stubbed."),
  );

  app.post("/api/backlog/triage", async ({ body }) =>
    entityResponse(
      { id: "backlog-triage-stub", ...(body || {}) },
      "Autonomous backlog triage is stubbed.",
    ),
  );

  app.get("/api/backlog/priorities", async () =>
    entityResponse(
      {
        classes: ["critical", "high", "medium", "low"],
      },
      "Backlog priority classes are stubbed.",
    ),
  );
}
