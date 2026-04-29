export function preCommitHook(payload: { repo: string; refs: string[] }) {
  return {
    ok: true,
    message: `Pre-commit hook accepted for ${payload.repo}`,
    warnings: [],
  };
}

export function postPushHook(payload: { repo: string; userId: number; changes: string[] }) {
  return {
    ok: true,
    message: `Post-push hook queued for ${payload.repo}`,
  };
}

export function pullRequestCreated(payload: { repo: string; prId: string }) {
  return {
    ok: true,
    message: `Pull request ${payload.prId} queued for Nexus AI review`,
  };
}
