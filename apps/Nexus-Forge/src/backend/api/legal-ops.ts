import { entityResponse, listResponse } from "./contracts";

export function registerLegalOpsRoutes(app: any) {
  app.get("/api/legal/contracts", async () => listResponse([], "Contract registry is stubbed."));
  app.post("/api/legal/contracts", async ({ body }: any) =>
    entityResponse({ id: "contract-stub", ...(body || {}) }, "Contract intake is stubbed.")
  );

  app.get("/api/legal/approvals", async () => listResponse([], "Legal approval workflow is stubbed."));
  app.post("/api/legal/approvals", async ({ body }: any) =>
    entityResponse({ id: "legal-approval-stub", ...(body || {}) }, "Legal approval routing is stubbed.")
  );

  app.get("/api/legal/holds", async () => listResponse([], "Legal hold catalog is stubbed."));
  app.get("/api/legal/discovery", async () =>
    entityResponse({ requests: [], exports: [] }, "E-discovery workflow is stubbed.")
  );
}
