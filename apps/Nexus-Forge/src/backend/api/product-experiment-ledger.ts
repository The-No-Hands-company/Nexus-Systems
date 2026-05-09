import { entityResponse, listResponse } from "./contracts";

export function registerProductExperimentLedgerRoutes(app: ForgeRouteApp) {
  app.get("/api/experiment-ledger/entries", async () =>
    listResponse([], "Experiment ledger entries are stubbed."),
  );
  app.post("/api/experiment-ledger/entries", async ({ body }) =>
    entityResponse(
      { id: "experiment-ledger-entry-stub", ...(body || {}) },
      "Ledger entry creation is stubbed.",
    ),
  );

  app.get("/api/experiment-ledger/outcomes", async () =>
    listResponse([], "Experiment outcomes feed is stubbed."),
  );
  app.post("/api/experiment-ledger/approvals", async ({ body }) =>
    entityResponse(
      { id: "experiment-ledger-approval-stub", payload: body || {} },
      "Approval workflow is stubbed.",
    ),
  );

  app.get("/api/experiment-ledger/tags", async () =>
    listResponse([], "Experiment taxonomy tags are stubbed."),
  );
}
