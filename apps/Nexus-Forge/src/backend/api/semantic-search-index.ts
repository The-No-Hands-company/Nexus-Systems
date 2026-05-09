import { entityResponse, listResponse } from "./contracts";

export function registerSemanticSearchIndexRoutes(app: ForgeRouteApp) {
  app.get("/api/semantic-search/index", async () => listResponse([], "Semantic search index"));
  app.post("/api/semantic-search/index-content", async ({ body }) =>
    entityResponse({ id: "idx-stub", ...(body || {}) }, "Index content for search"),
  );
  app.get("/api/semantic-search/queries", async () => listResponse([], "Semantic query history"));
  app.post("/api/semantic-search/query", async ({ body }) =>
    entityResponse({ id: "query-stub", payload: body || {} }, "Execute semantic search"),
  );
  app.get("/api/semantic-search/stats", async () =>
    entityResponse({ indexed: 0, ready: false }, "Search index statistics"),
  );
}
