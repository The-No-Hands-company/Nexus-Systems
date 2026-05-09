import { entityResponse, listResponse } from "./contracts";

export function registerEventStreamRoutes(app: ForgeRouteApp) {
  app.get("/api/events/channels", async () =>
    entityResponse(
      {
        channels: ["repo.activity", "pr.updates", "ci.runs", "agent.tasks"],
        transport: ["sse", "websocket"],
      },
      "Real-time channel catalog is stubbed.",
    ),
  );

  app.get("/api/events/subscriptions", async () =>
    listResponse([], "Subscription management is stubbed."),
  );
  app.post("/api/events/subscriptions", async ({ body }) =>
    entityResponse(
      { id: "subscription-stub", ...(body || {}) },
      "Subscription creation is stubbed.",
    ),
  );

  app.post("/api/events/publish", async ({ body }) =>
    entityResponse(
      { eventId: "event-stub", payload: body || {} },
      "Event publishing endpoint is stubbed.",
    ),
  );

  app.get("/api/events/replay", async () =>
    listResponse([], "Historical event replay is stubbed."),
  );
}
