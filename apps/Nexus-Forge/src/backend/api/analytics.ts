export function registerAnalyticsRoutes(app: ForgeRouteApp) {
  app.get("/api/analytics/usage", async () => ({
    dailyActiveUsers: 0,
    activeRepositories: 0,
    pullRequestVelocity: 0,
    note: "Usage analytics is stubbed.",
  }));

  app.get("/api/analytics/engagement", async () => ({
    insights: [],
    note: "Engagement insights are stubbed.",
  }));

  app.get("/api/analytics/reliability", async () => ({
    uptimePct: 100,
    errorBudgetBurn: 0,
    incidents: [],
    note: "Reliability analytics is stubbed.",
  }));

  app.post("/api/analytics/reports", async ({ body }) => ({
    ok: true,
    reportId: "report-stub",
    options: body || {},
  }));
}
