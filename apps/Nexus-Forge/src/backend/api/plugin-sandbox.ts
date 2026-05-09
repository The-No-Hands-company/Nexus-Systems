import { entityResponse, listResponse } from "./contracts";

export function registerPluginSandboxRoutes(app: ForgeRouteApp) {
  app.get("/api/plugins/registry", async () => listResponse([], "Plugin registry is stubbed."));
  app.post("/api/plugins/registry", async ({ body }) =>
    entityResponse({ id: "plugin-stub", ...(body || {}) }, "Plugin registration is stubbed."),
  );

  app.get("/api/plugins/sandboxes", async () =>
    listResponse([], "Plugin sandbox pool is stubbed."),
  );
  app.post("/api/plugins/sandboxes", async ({ body }) =>
    entityResponse({ id: "sandbox-stub", ...(body || {}) }, "Sandbox allocation is stubbed."),
  );

  app.get("/api/plugins/policies", async () =>
    entityResponse(
      {
        policies: ["network-isolation", "resource-quota", "permission-boundary"],
      },
      "Plugin policy model is stubbed.",
    ),
  );
}
