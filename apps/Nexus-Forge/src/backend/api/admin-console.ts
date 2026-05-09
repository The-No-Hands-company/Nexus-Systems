import { entityResponse, listResponse } from "./contracts";

export function registerAdminConsoleRoutes(app: ForgeRouteApp) {
  app.get("/api/admin-console/panels", async () =>
    entityResponse(
      {
        panels: ["system-health", "security-overview", "queue-depth", "storage-usage"],
      },
      "Admin panel registry is stubbed.",
    ),
  );

  app.get("/api/admin-console/jobs", async () =>
    listResponse([], "Administrative background jobs are stubbed."),
  );
  app.post("/api/admin-console/jobs", async ({ body }) =>
    entityResponse({ id: "admin-job-stub", ...(body || {}) }, "Admin job scheduling is stubbed."),
  );

  app.get("/api/admin-console/maintenance", async () =>
    entityResponse({ mode: "off", windows: [] }, "Maintenance window orchestration is stubbed."),
  );
  app.post("/api/admin-console/maintenance", async ({ body }) => {
    const payload =
      typeof body === "object" && body !== null ? (body as Record<string, unknown>) : {};

    return entityResponse(
      { mode: String(payload.mode || "scheduled"), window: payload },
      "Maintenance updates are stubbed.",
    );
  });
}
