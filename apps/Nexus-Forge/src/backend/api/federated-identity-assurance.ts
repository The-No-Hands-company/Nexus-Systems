import { entityResponse, listResponse } from "./contracts";

export function registerFederatedIdentityAssuranceRoutes(app: ForgeRouteApp) {
  app.get("/api/federated-identity/providers", async () =>
    listResponse([], "Federated identity provider catalog is stubbed."),
  );
  app.post("/api/federated-identity/providers", async ({ body }) =>
    entityResponse(
      { id: "federated-identity-provider-stub", ...(body || {}) },
      "Provider registration is stubbed.",
    ),
  );

  app.get("/api/federated-identity/trust-policies", async () =>
    listResponse([], "Trust policy catalog is stubbed."),
  );
  app.post("/api/federated-identity/assertions", async ({ body }) =>
    entityResponse(
      { id: "federated-identity-assertion-stub", payload: body || {} },
      "Assertion verification is stubbed.",
    ),
  );

  app.get("/api/federated-identity/protocols", async () =>
    entityResponse(
      {
        protocols: ["oidc", "saml", "scim", "jwt"],
      },
      "Federated identity protocol matrix is stubbed.",
    ),
  );
}
