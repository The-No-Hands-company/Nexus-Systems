import { entityResponse, listResponse } from "./contracts";

export function registerSecretsVaultRoutes(app: any) {
  app.get("/api/secrets/providers", async () =>
    entityResponse(
      {
        providers: ["vault", "aws-kms", "gcp-kms", "azure-key-vault"],
      },
      "Secret provider registry is stubbed."
    )
  );

  app.get("/api/secrets/namespaces", async () => listResponse([], "Secret namespace catalog is stubbed."));
  app.post("/api/secrets/namespaces", async ({ body }: any) =>
    entityResponse({ id: "secret-namespace-stub", ...(body || {}) }, "Secret namespace creation is stubbed.")
  );

  app.get("/api/secrets/rotations", async () => listResponse([], "Secret rotation schedules are stubbed."));
  app.post("/api/secrets/rotate", async ({ body }: any) =>
    entityResponse({ rotationId: "rotation-stub", payload: body || {} }, "Secret rotation workflow is stubbed.")
  );
}
