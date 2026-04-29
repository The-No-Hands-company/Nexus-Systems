export function registerComplianceRoutes(app: any) {
  app.get("/api/compliance/frameworks", async () => ({
    frameworks: ["SOC2", "ISO27001", "GDPR", "HIPAA"],
    note: "Compliance frameworks are stubbed.",
  }));

  app.get("/api/compliance/controls", async () => ({
    controls: [],
    note: "Control catalog is stubbed.",
  }));

  app.get("/api/compliance/evidence", async () => ({
    evidence: [],
    note: "Evidence collection is stubbed.",
  }));

  app.post("/api/compliance/audits", async ({ body }: any) => ({
    ok: true,
    auditId: "audit-stub",
    scope: body || {},
  }));

  app.get("/api/compliance/dsar", async () => ({
    requests: [],
    note: "Data subject access request workflows are stubbed.",
  }));
}
