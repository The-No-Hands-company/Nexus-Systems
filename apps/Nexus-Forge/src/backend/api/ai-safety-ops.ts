import { entityResponse, listResponse } from "./contracts";

export function registerAISafetyOpsRoutes(app: ForgeRouteApp) {
  app.get("/api/ai-safety/policies", async () =>
    listResponse([], "AI safety policy registry is stubbed."),
  );
  app.post("/api/ai-safety/policies", async ({ body }) =>
    entityResponse(
      { id: "ai-safety-policy-stub", ...(body || {}) },
      "AI safety policy authoring is stubbed.",
    ),
  );

  app.get("/api/ai-safety/evaluations", async () =>
    listResponse([], "Model safety evaluations are stubbed."),
  );
  app.post("/api/ai-safety/incidents", async ({ body }) =>
    entityResponse(
      { id: "ai-safety-incident-stub", ...(body || {}) },
      "AI incident recording is stubbed.",
    ),
  );

  app.get("/api/ai-safety/controls", async () =>
    entityResponse(
      {
        controls: ["prompt-shield", "output-filter", "human-approval", "drift-watch"],
      },
      "AI safety controls are stubbed.",
    ),
  );
}
