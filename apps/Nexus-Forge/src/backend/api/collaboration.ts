export function registerCollaborationRoutes(app: ForgeRouteApp) {
  app.get("/api/discussions", async () => ({
    discussions: [],
    nextCursor: null,
    note: "Discussions are stubbed.",
  }));
  app.post("/api/discussions", async ({ body }) => ({
    ok: true,
    discussion: { id: "disc-stub-1", ...(body || {}) },
  }));

  app.get("/api/discussions/:id/comments", async ({ params }) => ({
    discussionId: params.id,
    comments: [],
    note: "Discussion comments are stubbed.",
  }));
  app.post("/api/discussions/:id/comments", async ({ params, body }) => ({
    ok: true,
    discussionId: params.id,
    comment: { id: "comment-stub", ...(body || {}) },
  }));

  app.get("/api/announcements", async () => ({
    announcements: [],
    note: "Announcements board is stubbed.",
  }));
  app.post("/api/announcements", async ({ body }) => ({
    ok: true,
    announcement: body || {},
  }));

  app.get("/api/mentions", async () => ({
    mentions: [],
    unread: 0,
    note: "Mentions feed is stubbed.",
  }));
  app.post("/api/reactions", async ({ body }) => ({
    ok: true,
    reaction: body || {},
  }));

  app.get("/api/changelog", async () => ({
    entries: [],
    note: "Project changelog publishing is stubbed.",
  }));
}
