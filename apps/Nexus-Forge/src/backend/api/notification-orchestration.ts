import { entityResponse, listResponse } from "./contracts";

export function registerNotificationOrchestrationRoutes(app: ForgeRouteApp) {
  app.get("/api/notifications/channels", async () => listResponse([], "Notification channels"));
  app.post("/api/notifications/send", async ({ body }) =>
    entityResponse({ id: "notif-stub", ...(body || {}) }, "Send notification"),
  );
  app.get("/api/notifications/templates", async () => listResponse([], "Notification templates"));
  app.post("/api/notifications/schedule", async ({ body }) =>
    entityResponse({ id: "schedule-stub", payload: body || {} }, "Schedule notification"),
  );
  app.get("/api/notifications/delivery", async () =>
    entityResponse({ delivered: 0, failed: 0 }, "Delivery statistics"),
  );
}
