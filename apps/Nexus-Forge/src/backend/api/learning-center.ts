import { entityResponse, listResponse } from "./contracts";

export function registerLearningCenterRoutes(app: any) {
  app.get("/api/learning/tracks", async () => listResponse([], "Learning track catalog is stubbed."));
  app.post("/api/learning/tracks", async ({ body }: any) =>
    entityResponse({ id: "learning-track-stub", ...(body || {}) }, "Learning track creation is stubbed.")
  );

  app.get("/api/learning/courses", async () => listResponse([], "Course registry is stubbed."));
  app.get("/api/learning/certifications", async () => listResponse([], "Certification paths are stubbed."));

  app.get("/api/learning/paths", async () =>
    entityResponse(
      {
        paths: ["forge-admin", "platform-engineer", "security-lead", "release-manager"],
      },
      "Learning paths are stubbed."
    )
  );
}
