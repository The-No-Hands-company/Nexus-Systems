import { entityResponse, listResponse } from "./contracts";

export function registerPagesHostingRoutes(app: any) {
  app.get("/api/pages-hosting/sites", async () =>
    listResponse([], "Project and organization static hosting sites"));

  app.get("/api/pages-hosting/domains", async () =>
    listResponse([], "Custom domain and TLS provisioning stubs"));

  app.get("/api/pages-hosting/deployments", async () =>
    entityResponse({ source: "branch-or-ci", deploymentHistory: "stubbed" }, "Pages deployment status"));

  app.post("/api/pages-hosting/stub", async ({ body }: any) =>
    entityResponse({ id: "pages-hosting-stub", payload: body || {} }, "Pages hosting stub request accepted"));
}