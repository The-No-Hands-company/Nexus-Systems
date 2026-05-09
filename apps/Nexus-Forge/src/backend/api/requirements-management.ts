import { entityResponse, listResponse } from "./contracts";

export function registerRequirementsManagementRoutes(app: ForgeRouteApp) {
  app.get("/api/requirements-management/requirements", async () =>
    listResponse([], "Requirement objects and status lifecycle parity"),
  );

  app.get("/api/requirements-management/traceability", async () =>
    listResponse([], "Requirement-to-issue and requirement-to-test traceability parity"),
  );

  app.get("/api/requirements-management/reports", async () =>
    entityResponse(
      { complianceMatrix: "stubbed", csvImport: "stubbed" },
      "Requirements compliance reporting parity",
    ),
  );

  app.post("/api/requirements-management/stub", async ({ body }) =>
    entityResponse(
      { id: "requirements-management-stub", payload: body || {} },
      "Requirements management parity stub request accepted",
    ),
  );
}
