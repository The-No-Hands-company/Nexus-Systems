export function createDeployPayload(repo: string, ref: string, commit: string) {
  return {
    repo,
    ref,
    commit,
    trigger: "forge-webhook",
    timestamp: new Date().toISOString(),
  };
}

export function callDeployService(url: string, payload: Record<string, unknown>) {
  return {
    endpoint: url,
    payload,
    status: "queued",
    message: "Deploy service call is stubbed.",
  };
}
