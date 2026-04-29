import { entityResponse, listResponse } from "./contracts";

export function registerEdgeRoutes(app: any) {
  app.get("/api/edge/regions", async () =>
    entityResponse(
      {
        regions: ["us-east", "eu-west", "ap-south"],
        activeRegion: "us-east",
      },
      "Edge region topology is stubbed."
    )
  );

  app.get("/api/edge/replicas", async () => listResponse([], "Edge replica registry is stubbed."));
  app.post("/api/edge/replicas", async ({ body }: any) =>
    entityResponse({ id: "replica-stub", ...(body || {}) }, "Replica registration is stubbed.")
  );

  app.get("/api/edge/routing", async () =>
    entityResponse({ policy: "latency-first", fallback: "primary" }, "Traffic routing policy is stubbed.")
  );

  app.post("/api/edge/invalidate", async ({ body }: any) =>
    entityResponse({ invalidationId: "invalidate-stub", scope: body || {} }, "Cache invalidation is stubbed.")
  );
}
