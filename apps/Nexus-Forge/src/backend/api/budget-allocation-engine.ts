import { entityResponse, listResponse } from "./contracts";

export function registerBudgetAllocationEngineRoutes(app: any) {
  app.get("/api/budget-allocation/budgets", async () =>
    listResponse([], "Budget allocations and plans"));
  app.post("/api/budget-allocation/create", async ({ body }: any) =>
    entityResponse({ id: "budget-stub", ...(body || {}) }, "Create budget allocation"));
  app.get("/api/budget-allocation/tracking", async () =>
    listResponse([], "Budget spend tracking"));
  app.post("/api/budget-allocation/forecast", async ({ body }: any) =>
    entityResponse({ id: "forecast-stub", payload: body || {} }, "Forecast budget spend"));
  app.get("/api/budget-allocation/health", async () =>
    entityResponse({ utilization: 0, trend: "stable" }, "Budget health metrics"));
}
