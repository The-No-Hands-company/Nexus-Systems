import { entityResponse, listResponse } from "./contracts";

export function registerOperationalNarrativeStudioRoutes(app: ForgeRouteApp) {
  app.get("/api/operational-narrative/stories", async () =>
    listResponse([], "Operational narrative stories are stubbed."),
  );
  app.post("/api/operational-narrative/stories", async ({ body }) =>
    entityResponse(
      { id: "operational-story-stub", ...(body || {}) },
      "Narrative story authoring is stubbed.",
    ),
  );

  app.get("/api/operational-narrative/briefs", async () =>
    listResponse([], "Operational brief feed is stubbed."),
  );
  app.post("/api/operational-narrative/summaries", async ({ body }) =>
    entityResponse(
      { id: "operational-summary-stub", payload: body || {} },
      "Narrative summary generation is stubbed.",
    ),
  );

  app.get("/api/operational-narrative/audiences", async () =>
    listResponse([], "Audience profiles are stubbed."),
  );
}
