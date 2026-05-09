import { entityResponse, listResponse } from "./contracts";

export function registerProjectBoardsFullDepthRoutes(app: ForgeRouteApp) {
  app.get("/api/project-boards-full-depth/views", async () =>
    listResponse(["kanban", "table", "roadmap", "portfolio"], "Board view modes for parity"),
  );

  app.get("/api/project-boards-full-depth/automation", async () =>
    listResponse([], "Board automation, triggers, and workflow parity"),
  );

  app.get("/api/project-boards-full-depth/metrics", async () =>
    entityResponse(
      { burndown: "stubbed", velocity: "stubbed", deliveryPlans: "stubbed" },
      "Board analytics parity",
    ),
  );

  app.post("/api/project-boards-full-depth/stub", async ({ body }) =>
    entityResponse(
      { id: "project-boards-depth-stub", payload: body || {} },
      "Project boards depth parity stub request accepted",
    ),
  );
}
