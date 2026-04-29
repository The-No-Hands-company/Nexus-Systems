import { entityResponse, listResponse } from "./contracts";

export function registerReleaseEngineeringRoutes(app: any) {
  app.get("/api/release/channels", async () =>
    entityResponse(
      {
        channels: ["alpha", "beta", "stable", "lts"],
      },
      "Release channel registry is stubbed."
    )
  );

  app.get("/api/release/trains", async () => listResponse([], "Release train catalog is stubbed."));
  app.post("/api/release/trains", async ({ body }: any) =>
    entityResponse({ id: "release-train-stub", ...(body || {}) }, "Release train scheduling is stubbed.")
  );

  app.get("/api/release/candidates", async () => listResponse([], "Release candidate workflow is stubbed."));
  app.post("/api/release/promote", async ({ body }: any) =>
    entityResponse({ promotionId: "promote-stub", payload: body || {} }, "Release promotion is stubbed.")
  );

  app.get("/api/release/notes", async () => listResponse([], "Release note generation is stubbed."));
}
