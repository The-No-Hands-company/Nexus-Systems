import { entityResponse, listResponse } from "./contracts";

export function registerPartnerEcosystemRoutes(app: any) {
  app.get("/api/partners/directory", async () => listResponse([], "Partner directory is stubbed."));
  app.post("/api/partners/directory", async ({ body }: any) =>
    entityResponse({ id: "partner-stub", ...(body || {}) }, "Partner onboarding is stubbed.")
  );

  app.get("/api/partners/programs", async () => listResponse([], "Partner program catalog is stubbed."));
  app.get("/api/partners/deals", async () => listResponse([], "Partner deal registration is stubbed."));

  app.get("/api/partners/tiering", async () =>
    entityResponse(
      {
        tiers: ["registered", "silver", "gold", "platinum"],
      },
      "Partner tiering model is stubbed."
    )
  );
}
