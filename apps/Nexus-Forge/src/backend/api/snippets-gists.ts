import { entityResponse, listResponse } from "./contracts";

export function registerSnippetsGistsRoutes(app: ForgeRouteApp) {
  app.get("/api/snippets-gists/list", async () =>
    listResponse([], "Snippet/gist catalog across public, internal, and secret scopes"),
  );

  app.get("/api/snippets-gists/capabilities", async () =>
    listResponse([], "Forking, embedding, version history, and comment thread stubs"),
  );

  app.post("/api/snippets-gists/create", async ({ body }) =>
    entityResponse({ id: "snippet-stub", ...(body || {}) }, "Snippet/gist stub create accepted"),
  );
}
