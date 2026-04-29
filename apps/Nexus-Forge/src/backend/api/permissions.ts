import type { Elysia } from "elysia";

export function permissionRoutes(app: Elysia) {
  app.get("/api/permissions/check", async ({ query }) => {
    const repo = query.repo || "unknown";
    return {
      repo,
      read: true,
      write: true,
      admin: false,
      note: "Permission checks are stubbed. Implement team permission checks later.",
    };
  });

  app.get("/api/teams/:teamId/members", async ({ params }) => {
    return { teamId: params.teamId, members: [] };
  });
}
