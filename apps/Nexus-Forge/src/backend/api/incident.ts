export function registerIncidentRoutes(app: any) {
  app.get("/api/incidents", async () => ({ incidents: [], note: "Incident management is stubbed." }));
  app.post("/api/incidents", async ({ body }: any) => ({ ok: true, incident: { id: "incident-stub", ...(body || {}) } }));

  app.get("/api/incidents/postmortems", async () => ({ postmortems: [], note: "Postmortem workflow is stubbed." }));
  app.post("/api/incidents/postmortems", async ({ body }: any) => ({ ok: true, postmortem: body || {} }));

  app.get("/api/incidents/status-page", async () => ({
    status: "operational",
    components: [],
    note: "Public status page is stubbed.",
  }));

  app.get("/api/incidents/oncall", async () => ({ schedules: [], rotations: [], note: "On-call rotations are stubbed." }));
}
