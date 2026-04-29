export function registerMobileRoutes(app: any) {
  app.get("/api/mobile/sessions", async () => ({
    sessions: [],
    note: "Mobile device session tracking is stubbed.",
  }));

  app.get("/api/mobile/push-tokens", async () => ({
    tokens: [],
    note: "Push token registry is stubbed.",
  }));

  app.post("/api/mobile/push/send", async ({ body }: any) => ({
    ok: true,
    notificationId: "push-stub",
    payload: body || {},
  }));

  app.get("/api/mobile/layouts", async () => ({
    layouts: [
      { name: "repository-list", variant: "compact" },
      { name: "pull-request", variant: "reader" },
      { name: "feature-catalog", variant: "cards" },
    ],
    note: "Mobile UI layout registry is stubbed.",
  }));
}
