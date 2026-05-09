import { entityResponse, listResponse } from "./contracts";

export function registerStakeholderCommunicationIntelligenceRoutes(app: ForgeRouteApp) {
  app.get("/api/stakeholder-comms/audiences", async () =>
    listResponse([], "Stakeholder audiences are stubbed."),
  );
  app.post("/api/stakeholder-comms/audiences", async ({ body }) =>
    entityResponse(
      { id: "stakeholder-audience-stub", ...(body || {}) },
      "Audience definition is stubbed.",
    ),
  );

  app.get("/api/stakeholder-comms/briefings", async () =>
    listResponse([], "Stakeholder briefing feed is stubbed."),
  );
  app.post("/api/stakeholder-comms/recommendations", async ({ body }) =>
    entityResponse(
      { id: "stakeholder-recommendation-stub", payload: body || {} },
      "Communication recommendation is stubbed.",
    ),
  );

  app.get("/api/stakeholder-comms/signals", async () =>
    entityResponse(
      {
        signals: ["sentiment", "alignment", "urgency", "risk-awareness"],
      },
      "Stakeholder communication signals are stubbed.",
    ),
  );
}
