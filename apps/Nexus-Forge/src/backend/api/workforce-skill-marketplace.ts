import { entityResponse, listResponse } from "./contracts";

export function registerWorkforceSkillMarketplaceRoutes(app: ForgeRouteApp) {
  app.get("/api/workforce-skills/skills", async () =>
    listResponse([], "Workforce skill catalog is stubbed."),
  );
  app.post("/api/workforce-skills/skills", async ({ body }) =>
    entityResponse(
      { id: "workforce-skill-stub", ...(body || {}) },
      "Skill listing authoring is stubbed.",
    ),
  );

  app.get("/api/workforce-skills/demands", async () =>
    listResponse([], "Skill demand signals are stubbed."),
  );
  app.post("/api/workforce-skills/matches", async ({ body }) =>
    entityResponse(
      { id: "workforce-skill-match-stub", payload: body || {} },
      "Skill match orchestration is stubbed.",
    ),
  );

  app.get("/api/workforce-skills/pathways", async () =>
    listResponse([], "Skill pathway recommendations are stubbed."),
  );
}
