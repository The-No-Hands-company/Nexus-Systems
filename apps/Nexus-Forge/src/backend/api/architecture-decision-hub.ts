import { entityResponse, listResponse } from "./contracts";

export function registerArchitectureDecisionHubRoutes(app: ForgeRouteApp) {
  app.get("/api/adr/records", async () =>
    listResponse([], "Architecture decision records are stubbed."),
  );
  app.post("/api/adr/records", async ({ body }) =>
    entityResponse({ id: "adr-record-stub", ...(body || {}) }, "ADR creation workflow is stubbed."),
  );

  app.get("/api/adr/reviews", async () =>
    listResponse([], "Architecture review board workflow is stubbed."),
  );
  app.get("/api/adr/traceability", async () =>
    listResponse([], "Decision traceability graph is stubbed."),
  );

  app.get("/api/adr/templates", async () =>
    entityResponse(
      {
        templates: ["lightweight", "strategic", "security-impact", "migration"],
      },
      "ADR template catalog is stubbed.",
    ),
  );
}
