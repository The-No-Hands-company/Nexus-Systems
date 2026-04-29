export function registerMonetizationRoutes(app: any) {
  app.get("/api/billing/plans", async () => ({
    plans: [
      { id: "free", name: "Free", seats: 5 },
      { id: "team", name: "Team", seats: 50 },
      { id: "enterprise", name: "Enterprise", seats: -1 },
    ],
    note: "Plan catalog is stubbed.",
  }));

  app.get("/api/billing/subscriptions", async () => ({
    subscriptions: [],
    note: "Subscription lifecycle is stubbed.",
  }));

  app.post("/api/billing/subscriptions", async ({ body }: any) => ({
    ok: true,
    subscription: {
      id: "sub-stub",
      status: "trialing",
      ...(body || {}),
    },
  }));

  app.get("/api/billing/invoices", async () => ({ invoices: [], note: "Invoice ledger is stubbed." }));
  app.get("/api/billing/usage", async () => ({ usage: [], note: "Usage metering is stubbed." }));
  app.get("/api/billing/quotas", async () => ({ quotas: [], note: "Quota policies are stubbed." }));
}
