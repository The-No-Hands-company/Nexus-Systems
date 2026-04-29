import { entityResponse, listResponse } from "./contracts";

export function registerCommitSigningVaultRoutes(app: any) {
  app.get("/api/commit-signing/keys", async () =>
    listResponse([], "GPG and signing keys"));
  app.post("/api/commit-signing/register", async ({ body }: any) =>
    entityResponse({ id: "key-stub", ...(body || {}) }, "Register signing key"));
  app.get("/api/commit-signing/verified", async () =>
    listResponse([], "Verified commits"));
  app.post("/api/commit-signing/verify", async ({ body }: any) =>
    entityResponse({ id: "verify-stub", payload: body || {} }, "Verify commit signature"));
  app.get("/api/commit-signing/policy", async () =>
    entityResponse({ required: false, algorithms: ["RSA", "ECDSA", "Ed25519"] }, "Signing policy"));
}
