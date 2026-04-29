import { entityResponse, listResponse } from "./contracts";

export function registerExecutiveCommandCenterRoutes(app: any) {
  app.get("/api/executive/briefings", async () => listResponse([], "Executive briefing feed is stubbed."));
  app.get("/api/executive/decisions", async () => listResponse([], "Decision log is stubbed."));

  app.post("/api/executive/decisions", async ({ body }: any) =>
    entityResponse({ id: "executive-decision-stub", ...(body || {}) }, "Decision capture workflow is stubbed.")
  );

  app.get("/api/executive/signals", async () =>
    entityResponse(
      {
        channels: ["operations", "revenue", "trust", "innovation"],
      },
      "Executive signal channels are stubbed."
    )
  );
}
