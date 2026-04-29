import { entityResponse, listResponse } from "./contracts";

export function registerLocalizationRoutes(app: any) {
  app.get("/api/localization/locales", async () =>
    entityResponse({ supported: ["en", "es", "de", "fr", "ja"], default: "en" }, "Locale registry is stubbed.")
  );

  app.get("/api/localization/strings", async () => listResponse([], "Translation key catalog is stubbed."));
  app.post("/api/localization/strings", async ({ body }: any) =>
    entityResponse({ id: "i18n-string-stub", ...(body || {}) }, "Translation string updates are stubbed.")
  );

  app.get("/api/localization/reviews", async () => listResponse([], "Localization review queue is stubbed."));
  app.post("/api/localization/reviews", async ({ body }: any) =>
    entityResponse({ id: "i18n-review-stub", ...(body || {}) }, "Localization review assignment is stubbed.")
  );
}
