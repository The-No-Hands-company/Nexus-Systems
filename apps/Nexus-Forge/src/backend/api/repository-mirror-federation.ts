import { entityResponse, listResponse } from "./contracts";

export function registerRepositoryMirrorFederationRoutes(app: any) {
  app.get("/api/repository-mirror-federation/mirrors", async () =>
    listResponse([], "Push and pull mirror topology across providers"));

  app.get("/api/repository-mirror-federation/migrations", async () =>
    listResponse([], "Migration plan stubs for issues, PRs, releases, and wiki"));

  app.get("/api/repository-mirror-federation/status", async () =>
    entityResponse({ providers: ["github", "gitlab", "gitea", "bitbucket"], syncState: "stubbed" }, "Mirror federation status"));

  app.post("/api/repository-mirror-federation/stub", async ({ body }: any) =>
    entityResponse({ id: "mirror-federation-stub", ...(body || {}) }, "Mirror federation stub request accepted"));
}