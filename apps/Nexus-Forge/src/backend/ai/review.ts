import { env } from "bun";

const AI_URL = env.NEXUS_AI_URL || "http://localhost:8000";

export interface ReviewResult {
  ok: boolean;
  summary: string;
  suggestions: string[];
}

export async function reviewPullRequest(
  repo: string,
  prId: string,
  _diff: string,
): Promise<ReviewResult> {
  // TODO: Integrate with Nexus AI service.
  return {
    ok: true,
    summary: `Review stub for PR ${prId} in ${repo}`,
    suggestions: ["Add tests for new functionality", "Validate permissions before merge"],
  };
}

export async function callNexusAI(payload: Record<string, unknown>) {
  // Placeholder for fetch-based AI calls.
  return {
    endpoint: AI_URL,
    payload,
    result: "stubbed response",
  };
}
