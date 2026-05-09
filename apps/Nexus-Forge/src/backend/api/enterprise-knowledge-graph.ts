import { entityResponse, listResponse } from "./contracts";

export function registerEnterpriseKnowledgeGraphRoutes(app: ForgeRouteApp) {
  app.get("/api/knowledge-graph/entities", async () =>
    listResponse([], "Knowledge graph entity catalog is stubbed."),
  );
  app.post("/api/knowledge-graph/entities", async ({ body }) =>
    entityResponse(
      { id: "knowledge-graph-entity-stub", ...(body || {}) },
      "Knowledge graph entity creation is stubbed.",
    ),
  );

  app.get("/api/knowledge-graph/relationships", async () =>
    listResponse([], "Knowledge graph relationships are stubbed."),
  );
  app.post("/api/knowledge-graph/inference", async ({ body }) =>
    entityResponse(
      { id: "knowledge-graph-inference-stub", payload: body || {} },
      "Knowledge inference workflow is stubbed.",
    ),
  );

  app.get("/api/knowledge-graph/views", async () =>
    listResponse([], "Knowledge graph view definitions are stubbed."),
  );
}
