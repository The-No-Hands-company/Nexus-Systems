import { entityResponse, listResponse } from "./contracts";

export function registerContentStudioRoutes(app: ForgeRouteApp) {
  app.get("/api/content/templates", async () =>
    listResponse([], "Content template registry is stubbed."),
  );
  app.post("/api/content/templates", async ({ body }) =>
    entityResponse(
      { id: "content-template-stub", ...(body || {}) },
      "Template authoring is stubbed.",
    ),
  );

  app.get("/api/content/assets", async () => listResponse([], "Content asset library is stubbed."));
  app.post("/api/content/assets", async ({ body }) =>
    entityResponse({ id: "asset-stub", ...(body || {}) }, "Asset upload lifecycle is stubbed."),
  );

  app.get("/api/content/workflows", async () =>
    listResponse([], "Content approval workflows are stubbed."),
  );
}
