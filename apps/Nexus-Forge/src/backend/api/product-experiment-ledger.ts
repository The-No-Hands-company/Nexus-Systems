import { entityResponse, listResponse } from "./contracts";

export function registerProductExperimentLedgerRoutes(app: any) {
  app.get("/api/experiment-ledger/entries", async () => listResponse([], "Experiment ledger entries are stubbed."));
  app.post("/api/experiment-ledger/entries", async ({ body }: any) =>
    entityResponse({ id: "experiment-ledger-entry-stub", ...(body || {}) }, "Ledger entry creation is stubbed.")
  );

  app.get("/api/experiment-ledger/outcomes", async () => listResponse([], "Experiment outcomes feed is stubbed."));
  app.post("/api/experiment-ledger/approvals", async ({ body }: any) =>
    entityResponse({ id: "experiment-ledger-approval-stub", payload: body || {} }, "Approval workflow is stubbed.")
  );

  app.get("/api/experiment-ledger/tags", async () => listResponse([], "Experiment taxonomy tags are stubbed."));
}
