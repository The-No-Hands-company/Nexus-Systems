import { entityResponse, listResponse } from "./contracts";

export function registerMachineTranslationPlatformRoutes(app: any) {
  app.get("/api/translation/languages", async () =>
    listResponse([], "Supported translation languages"));
  app.post("/api/translation/translate", async ({ body }: any) =>
    entityResponse({ id: "trans-stub", ...(body || {}) }, "Translate content"));
  app.get("/api/translation/projects", async () =>
    listResponse([], "Translation projects"));
  app.post("/api/translation/batch", async ({ body }: any) =>
    entityResponse({ id: "batch-stub", payload: body || {} }, "Batch translate content"));
  app.get("/api/translation/quality", async () =>
    entityResponse({ score: 0, languages: 0 }, "Translation quality metrics"));
}
