import { entityResponse, listResponse } from "./contracts";

export function registerCommunicationRoutes(app: ForgeRouteApp) {
  app.get("/api/comms/campaigns", async () =>
    listResponse([], "Communication campaigns are stubbed."),
  );
  app.post("/api/comms/campaigns", async ({ body }) =>
    entityResponse({ id: "campaign-stub", ...(body || {}) }, "Campaign creation is stubbed."),
  );

  app.get("/api/comms/templates", async () =>
    listResponse([], "Message template registry is stubbed."),
  );
  app.post("/api/comms/templates", async ({ body }) =>
    entityResponse({ id: "template-stub", ...(body || {}) }, "Template creation is stubbed."),
  );

  app.post("/api/comms/send", async ({ body }) =>
    entityResponse(
      { deliveryId: "delivery-stub", payload: body || {} },
      "Outbound communication dispatch is stubbed.",
    ),
  );

  app.get("/api/comms/deliveries", async () => listResponse([], "Delivery tracking is stubbed."));
}
