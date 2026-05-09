import { entityResponse, listResponse } from "./contracts";

export function registerDesignAssetManagementRoutes(app: ForgeRouteApp) {
  app.get("/api/design-asset-management/assets", async () =>
    listResponse([], "Design files and embeds parity (PNG, Sketch, Figma links)"),
  );

  app.get("/api/design-asset-management/review", async () =>
    listResponse([], "Pinned design comments and review state parity"),
  );

  app.get("/api/design-asset-management/versioning", async () =>
    entityResponse(
      { diffOverlay: "stubbed", versionHistory: "stubbed" },
      "Design versioning parity",
    ),
  );

  app.post("/api/design-asset-management/stub", async ({ body }) =>
    entityResponse(
      { id: "design-asset-management-stub", ...(body || {}) },
      "Design and asset management parity stub request accepted",
    ),
  );
}
