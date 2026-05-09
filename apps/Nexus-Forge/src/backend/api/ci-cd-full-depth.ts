import { entityResponse, listResponse } from "./contracts";

export function registerCiCdFullDepthRoutes(app: ForgeRouteApp) {
  app.get("/api/ci-cd-full-depth/runners", async () =>
    listResponse([], "Runner matrix for Linux/macOS/Windows, hosted and self-hosted"),
  );

  app.get("/api/ci-cd-full-depth/pipeline-features", async () =>
    listResponse([], "Pipeline semantics for matrix builds, DAG, gates, and reusable workflows"),
  );

  app.get("/api/ci-cd-full-depth/review-apps", async () =>
    listResponse([], "Review app lifecycle and branch preview environment stubs"),
  );

  app.get("/api/ci-cd-full-depth/deploy-tracking", async () =>
    entityResponse(
      { environments: ["dev", "staging", "prod"], approvals: "stubbed" },
      "Deployment tracking baseline",
    ),
  );

  app.post("/api/ci-cd-full-depth/stub", async ({ body }) =>
    entityResponse(
      { id: "ci-cd-depth-stub", ...(body || {}) },
      "CI/CD depth parity stub request accepted",
    ),
  );
}
