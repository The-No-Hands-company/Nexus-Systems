import { entityResponse, listResponse } from "./contracts";

export function registerCarbonAwareRuntimeRoutes(app: any) {
  app.get("/api/carbon-runtime/policies", async () => listResponse([], "Carbon-aware runtime policies are stubbed."));
  app.post("/api/carbon-runtime/policies", async ({ body }: any) =>
    entityResponse({ id: "carbon-runtime-policy-stub", ...(body || {}) }, "Carbon policy authoring is stubbed.")
  );

  app.get("/api/carbon-runtime/signals", async () => listResponse([], "Carbon intensity signal feeds are stubbed."));
  app.post("/api/carbon-runtime/schedules", async ({ body }: any) =>
    entityResponse({ id: "carbon-runtime-schedule-stub", payload: body || {} }, "Carbon-aware scheduling is stubbed.")
  );

  app.get("/api/carbon-runtime/modes", async () =>
    entityResponse(
      {
        modes: ["performance", "balanced", "eco", "emergency"],
      },
      "Carbon runtime modes are stubbed."
    )
  );
}
