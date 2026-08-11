/**
 * Same-origin calls to the dashboard server.
 *
 * Every path here is relative on purpose. The dashboard server proxies
 * /api/v1/auth/* to Nexus-Auth, so the browser never makes a cross-origin
 * request and never needs credentialed CORS. Hardcoding https://auth.<domain>
 * here would work in dev and fail in a browser for reasons that look like a
 * login bug.
 */

export type AppEntry = {
  id: string;
  name: string;
  description: string;
  url: string;
  health: "healthy" | "offline";
};

export type Me = { id: string; username: string; email: string; role: string };

/** Carries the server's machine-readable reason so pages can explain it. */
export class ApiError extends Error {
  constructor(public readonly reason: string, message?: string) {
    super(message ?? reason);
    this.name = "ApiError";
  }
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  let res: Response;
  try {
    res = await fetch(path, {
      ...init,
      credentials: "same-origin",
      headers: { "content-type": "application/json", ...(init?.headers ?? {}) },
    });
  } catch {
    // A thrown fetch is the network being unreachable, which is a different
    // problem from the server saying no and deserves a different message.
    throw new ApiError("network", "Could not reach the server. Check your connection and try again.");
  }

  const body = await res.json().catch(() => ({}) as Record<string, unknown>);
  if (!res.ok) {
    const reason = typeof (body as { error?: unknown }).error === "string"
      ? (body as { error: string }).error
      : `http_${res.status}`;
    throw new ApiError(reason, reason);
  }
  return body as T;
}

export function requestAccess(input: { username: string; email: string; note?: string }) {
  return request<{ user: { id: string; status: string }; claimCode: string }>(
    "/api/v1/auth/access-requests",
    { method: "POST", body: JSON.stringify(input) },
  );
}

export function claimAccount(input: { email: string; claimCode: string; password: string }) {
  return request<{ user: { id: string }; recoveryCodes: string[] }>(
    "/api/v1/auth/claim",
    { method: "POST", body: JSON.stringify(input) },
  );
}

export async function me(): Promise<Me | null> {
  try {
    const { user } = await request<{ user: Me }>("/api/v1/auth/me");
    return user;
  } catch {
    // Not signed in is the common case here, not an error worth surfacing.
    return null;
  }
}

export async function listApps(): Promise<AppEntry[]> {
  const { apps } = await request<{ apps: AppEntry[] }>("/api/apps");
  return apps;
}
