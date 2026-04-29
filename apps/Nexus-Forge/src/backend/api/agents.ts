export function registerAgentRoutes(app: any) {
  app.get("/api/agents/tasks", async () => ({ tasks: [], note: "Agent task queue is stubbed." }));
  app.post("/api/agents/tasks", async ({ body }: any) => ({
    ok: true,
    task: {
      id: "agent-task-stub",
      type: body?.type || "unknown",
      status: "queued",
      payload: body?.payload || {},
    },
  }));

  app.get("/api/agents/tasks/:id", async ({ params }: any) => ({
    task: {
      id: params.id,
      status: "queued",
      type: "code_review",
      created_at: new Date().toISOString(),
    },
  }));

  app.post("/api/agents/tasks/:id/cancel", async ({ params }: any) => ({ ok: true, id: params.id, status: "cancelled" }));
  app.get("/api/agents/metrics", async () => ({
    agent_task_duration_ms: 0,
    agent_task_failures: 0,
    nexus_ai_calls_total: 0,
    ai_model_consensus_rate: 0,
  }));
}
