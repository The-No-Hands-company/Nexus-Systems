import { entityResponse, listResponse } from "./contracts";

export function registerChatOpsRoutes(app: ForgeRouteApp) {
  app.get("/api/chatops/channels", async () =>
    listResponse([], "ChatOps channel bindings are stubbed."),
  );
  app.post("/api/chatops/channels", async ({ body }) =>
    entityResponse({ id: "chatops-channel-stub", ...(body || {}) }, "Channel binding is stubbed."),
  );

  app.get("/api/chatops/commands", async () =>
    listResponse([], "Slash command registry is stubbed."),
  );
  app.post("/api/chatops/commands", async ({ body }) =>
    entityResponse(
      { id: "chatops-command-stub", ...(body || {}) },
      "Command registration is stubbed.",
    ),
  );

  app.post("/api/chatops/dispatch", async ({ body }) =>
    entityResponse(
      { deliveryId: "chatops-delivery-stub", payload: body || {} },
      "ChatOps dispatch is stubbed.",
    ),
  );

  app.get("/api/chatops/history", async () =>
    listResponse([], "ChatOps event history is stubbed."),
  );
}
