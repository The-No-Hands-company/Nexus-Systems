import { entityResponse, listResponse } from "./contracts";

export function registerApiManagementRoutes(app: ForgeRouteApp) {
  app.get("/api/api-management/keys", async () => listResponse([], "API key registry is stubbed."));
  app.post("/api/api-management/keys", async ({ body }) =>
    entityResponse({ id: "api-key-stub", ...(body || {}) }, "API key issuance is stubbed."),
  );

  app.get("/api/api-management/rate-limits", async () =>
    listResponse([], "Rate limit policy catalog is stubbed."),
  );
  app.post("/api/api-management/rate-limits", async ({ body }) =>
    entityResponse({ id: "rate-limit-stub", ...(body || {}) }, "Rate limit updates are stubbed."),
  );

  app.get("/api/api-management/oauth-clients", async () =>
    listResponse([], "OAuth client management is stubbed."),
  );
  app.post("/api/api-management/oauth-clients", async ({ body }) =>
    entityResponse(
      { id: "oauth-client-stub", ...(body || {}) },
      "OAuth client provisioning is stubbed.",
    ),
  );

  app.get("/api/api-management/audit", async () =>
    listResponse([], "API access audit feed is stubbed."),
  );
}
