import { entityResponse, listResponse } from "./contracts";

export function registerServiceCatalogRoutes(app: any) {
  app.get("/api/services/catalog", async () => listResponse([], "Service catalog is stubbed."));
  app.post("/api/services/catalog", async ({ body }: any) =>
    entityResponse({ id: "service-stub", ...(body || {}) }, "Service registration is stubbed.")
  );

  app.get("/api/services/dependencies", async () =>
    entityResponse({ graph: [], note: "Dependency graph is stubbed." }, "Dependency topology is stubbed.")
  );

  app.get("/api/services/ownership", async () => listResponse([], "Ownership mapping is stubbed."));
  app.post("/api/services/ownership", async ({ body }: any) =>
    entityResponse({ id: "ownership-stub", ...(body || {}) }, "Ownership assignment is stubbed.")
  );

  app.get("/api/services/slo", async () => listResponse([], "Service level objective registry is stubbed."));
}
