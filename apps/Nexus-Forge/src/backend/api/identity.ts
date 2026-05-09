import { entityResponse, listResponse } from "./contracts";

export function registerIdentityRoutes(app: ForgeRouteApp) {
  app.get("/api/identity/users", async () => listResponse([], "User directory is stubbed."));
  app.post("/api/identity/users", async ({ body }) =>
    entityResponse({ id: "user-stub", ...(body || {}) }, "User provisioning is stubbed."),
  );

  app.get("/api/identity/groups", async () => listResponse([], "Group management is stubbed."));
  app.post("/api/identity/groups", async ({ body }) =>
    entityResponse({ id: "group-stub", ...(body || {}) }, "Group creation is stubbed."),
  );

  app.get("/api/identity/roles", async () => listResponse([], "Role catalog is stubbed."));
  app.post("/api/identity/roles", async ({ body }) =>
    entityResponse({ id: "role-stub", ...(body || {}) }, "Custom role creation is stubbed."),
  );

  app.get("/api/identity/sessions", async () => listResponse([], "Session management is stubbed."));
  app.get("/api/identity/audit", async () => listResponse([], "Identity audit trail is stubbed."));
}
