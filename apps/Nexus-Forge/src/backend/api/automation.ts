export function registerAutomationRoutes(app: ForgeRouteApp) {
  app.get("/api/automation/rules", async () => ({
    rules: [],
    note: "Automation rules are stubbed.",
  }));
  app.post("/api/automation/rules", async ({ body }) => ({
    ok: true,
    rule: { id: "rule-stub", ...(body || {}) },
  }));

  app.get("/api/automation/schedules", async () => ({
    schedules: [],
    note: "Scheduled jobs are stubbed.",
  }));
  app.post("/api/automation/schedules", async ({ body }) => ({
    ok: true,
    schedule: body || {},
  }));

  app.get("/api/automation/templates", async () => ({
    templates: [],
    note: "Workflow template registry is stubbed.",
  }));
  app.post("/api/automation/templates", async ({ body }) => ({
    ok: true,
    template: { id: "template-stub", ...(body || {}) },
  }));

  app.post("/api/automation/dispatch", async ({ body }) => ({
    ok: true,
    dispatchId: "dispatch-stub",
    payload: body || {},
  }));
  app.get("/api/automation/history", async () => ({
    runs: [],
    note: "Automation run history is stubbed.",
  }));
}
