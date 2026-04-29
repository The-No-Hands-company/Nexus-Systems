import { entityResponse, listResponse } from "./contracts";

export function registerAdminConsoleRoutes(app: any) {
  app.get("/api/admin-console/panels", async () =>
    entityResponse(
      {
        panels: ["system-health", "security-overview", "queue-depth", "storage-usage"],
      },
      "Admin panel registry is stubbed."
    )
  );

  app.get("/api/admin-console/jobs", async () => listResponse([], "Administrative background jobs are stubbed."));
  app.post("/api/admin-console/jobs", async ({ body }: any) =>
    entityResponse({ id: "admin-job-stub", ...(body || {}) }, "Admin job scheduling is stubbed.")
  );

  app.get("/api/admin-console/maintenance", async () =>
    entityResponse({ mode: "off", windows: [] }, "Maintenance window orchestration is stubbed.")
  );
  app.post("/api/admin-console/maintenance", async ({ body }: any) =>
    entityResponse({ mode: body?.mode || "scheduled", window: body || {} }, "Maintenance updates are stubbed.")
  );
}
