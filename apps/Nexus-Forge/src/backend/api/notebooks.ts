import { entityResponse, listResponse } from "./contracts";

export function registerNotebookRoutes(app: any) {
  app.get("/api/notebooks", async () => listResponse([], "Notebook workspace index is stubbed."));
  app.post("/api/notebooks", async ({ body }: any) =>
    entityResponse({ id: "notebook-stub", ...(body || {}) }, "Notebook creation is stubbed.")
  );

  app.get("/api/notebooks/kernels", async () =>
    entityResponse({ kernels: ["python", "typescript", "bash"], remote: false }, "Kernel registry is stubbed.")
  );

  app.get("/api/notebooks/executions", async () => listResponse([], "Notebook execution history is stubbed."));
  app.post("/api/notebooks/executions", async ({ body }: any) =>
    entityResponse({ runId: "notebook-run-stub", input: body || {} }, "Notebook execution dispatch is stubbed.")
  );
}
