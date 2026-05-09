export function registerAgentRoutes(app: ForgeRouteApp) {
  app.get("/api/agents/tasks", async () => ({ tasks: [], note: "Agent task queue is stubbed." }));
  app.post("/api/agents/tasks", async ({ body }) => {
    const payload =
      typeof body === "object" && body !== null ? (body as Record<string, unknown>) : {};

    return {
      ok: true,
      task: {
        id: "agent-task-stub",
        type: String(payload.type || "unknown"),
        status: "queued",
        payload:
          typeof payload.payload === "object" && payload.payload !== null ? payload.payload : {},
      },
    };
  });

  app.get("/api/agents/tasks/:id", async ({ params }) => ({
    task: {
      id: params.id,
      status: "queued",
      type: "code_review",
      created_at: new Date().toISOString(),
    },
  }));

  app.post("/api/agents/tasks/:id/cancel", async ({ params }) => ({
    ok: true,
    id: params.id,
    status: "cancelled",
  }));
  app.get("/api/agents/metrics", async () => ({
    agent_task_duration_ms: 0,
    agent_task_failures: 0,
    nexus_ai_calls_total: 0,
    ai_model_consensus_rate: 0,
  }));
}
