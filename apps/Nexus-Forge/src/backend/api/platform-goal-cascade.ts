import { entityResponse, listResponse } from "./contracts";

export function registerPlatformGoalCascadeRoutes(app: any) {
  app.get("/api/goal-cascade/objectives", async () => listResponse([], "Goal cascade objective graph is stubbed."));
  app.post("/api/goal-cascade/objectives", async ({ body }: any) =>
    entityResponse({ id: "goal-cascade-objective-stub", ...(body || {}) }, "Goal objective authoring is stubbed.")
  );

  app.get("/api/goal-cascade/dependencies", async () => listResponse([], "Goal dependency mapping is stubbed."));
  app.post("/api/goal-cascade/alignment-checks", async ({ body }: any) =>
    entityResponse({ id: "goal-cascade-alignment-stub", payload: body || {} }, "Goal alignment checks are stubbed.")
  );

  app.get("/api/goal-cascade/views", async () => listResponse([], "Goal cascade views are stubbed."));
}
