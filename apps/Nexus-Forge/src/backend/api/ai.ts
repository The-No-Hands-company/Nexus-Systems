import type { Elysia } from "elysia";
import { reviewPullRequest } from "../ai/review";
import { generateIssuePR } from "../ai/issue";
import { createDeployPayload } from "../ai/deploy";

export function registerAIRoutes(app: Elysia) {
  app.post("/api/ai/review", async ({ body }) => {
    const payload = body as { repo: string; prId: string; diff: string };
    return reviewPullRequest(payload.repo, payload.prId, payload.diff || "");
  });

  app.post("/api/ai/issues/:issueId/generate-pr", async ({ body, params }) => {
    const payload = body as { title: string; description: string };
    return generateIssuePR(params.issueId as string, payload.title, payload.description || "");
  });

  app.post("/api/ai/deploy", async ({ body }) => {
    const payload = body as { repo: string; ref: string; commit: string };
    return createDeployPayload(payload.repo, payload.ref, payload.commit);
  });
}
