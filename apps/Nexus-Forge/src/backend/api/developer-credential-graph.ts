import { entityResponse, listResponse } from "./contracts";

export function registerDeveloperCredentialGraphRoutes(app: ForgeRouteApp) {
  app.get("/api/dev-credentials/nodes", async () =>
    listResponse([], "Developer credential nodes are stubbed."),
  );
  app.post("/api/dev-credentials/nodes", async ({ body }) =>
    entityResponse(
      { id: "dev-credential-node-stub", ...(body || {}) },
      "Credential node registration is stubbed.",
    ),
  );

  app.get("/api/dev-credentials/edges", async () =>
    listResponse([], "Credential relationship edges are stubbed."),
  );
  app.post("/api/dev-credentials/verifications", async ({ body }) =>
    entityResponse(
      { id: "dev-credential-verification-stub", payload: body || {} },
      "Credential verification is stubbed.",
    ),
  );

  app.get("/api/dev-credentials/policies", async () =>
    listResponse([], "Credential policy mapping is stubbed."),
  );
}
