import { entityResponse, listResponse } from "./contracts";

export function registerHybridWorkforceRouterRoutes(app: any) {
  app.get("/api/workforce/assignments", async () =>
    listResponse([], "Workforce task assignments"));
  app.post("/api/workforce/schedule", async ({ body }: any) =>
    entityResponse({ id: "schedule-stub", ...(body || {}) }, "Schedule workforce"));
  app.get("/api/workforce/capacity", async () =>
    listResponse([], "Available workforce capacity"));
  app.post("/api/workforce/optimize", async ({ body }: any) =>
    entityResponse({ id: "opt-stub", payload: body || {} }, "Optimize workforce routing"));
  app.get("/api/workforce/modes", async () =>
    entityResponse({ modes: ["on-site", "remote", "hybrid", "dynamic"] }, "Workforce modes"));
}
