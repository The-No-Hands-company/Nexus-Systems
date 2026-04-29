import { entityResponse, listResponse } from "./contracts";

export function registerChatOpsRoutes(app: any) {
  app.get("/api/chatops/channels", async () => listResponse([], "ChatOps channel bindings are stubbed."));
  app.post("/api/chatops/channels", async ({ body }: any) =>
    entityResponse({ id: "chatops-channel-stub", ...(body || {}) }, "Channel binding is stubbed.")
  );

  app.get("/api/chatops/commands", async () => listResponse([], "Slash command registry is stubbed."));
  app.post("/api/chatops/commands", async ({ body }: any) =>
    entityResponse({ id: "chatops-command-stub", ...(body || {}) }, "Command registration is stubbed.")
  );

  app.post("/api/chatops/dispatch", async ({ body }: any) =>
    entityResponse({ deliveryId: "chatops-delivery-stub", payload: body || {} }, "ChatOps dispatch is stubbed.")
  );

  app.get("/api/chatops/history", async () => listResponse([], "ChatOps event history is stubbed."));
}
