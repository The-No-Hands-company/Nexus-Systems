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

export type Session = {
  id: string;
  ipAddress: string;
  userAgent: string;
  createdAt: string;
};

export function listSessions() {
  return request<{ sessions: Session[] }>("/api/v1/auth/sessions").then((r) => r.sessions);
}

export function revokeSession(id: string) {
  return request<{ success: boolean }>(`/api/v1/auth/sessions/${encodeURIComponent(id)}/revoke`, {
    method: "POST",
  });
}

export function remainingRecoveryCodes() {
  return request<{ remaining: number }>("/api/v1/auth/recovery-codes").then((r) => r.remaining);
}

export function regenerateRecoveryCodes() {
  return request<{ recoveryCodes: string[] }>("/api/v1/auth/recovery-codes/regenerate", {
    method: "POST",
  }).then((r) => r.recoveryCodes);
}

/**
 * Always posts to the caller's own id. Self-service is authorised by the
 * current password, not by an admin permission.
 */
export function changePassword(userId: string, currentPassword: string, newPassword: string) {
  return request<{ success: boolean }>(
    `/api/v1/auth/users/${encodeURIComponent(userId)}/password`,
    { method: "POST", body: JSON.stringify({ currentPassword, newPassword }) },
  );
}

export type AccessRequest = {
  id: string;
  username: string;
  email: string;
  status: string;
  note?: string;
  createdAt: string;
};

/** Roles that may approve access requests — mirrors users:approve server-side. */
export const ADMIN_ROLES = ["founder", "admin"];

export function isAdmin(user: Me | null): boolean {
  return !!user && ADMIN_ROLES.includes(user.role);
}

export function listAccessRequests() {
  return request<{ requests: AccessRequest[] }>("/api/v1/auth/access-requests")
    .then((r) => r.requests);
}

export function decideAccessRequest(id: string, decision: "approve" | "reject") {
  return request<{ user: unknown }>(
    `/api/v1/auth/access-requests/${encodeURIComponent(id)}/${decision}`,
    { method: "POST", body: JSON.stringify({}) },
  );
}

export function createInvite(expiresInDays?: number) {
  return request<{ id: string; code: string; expiresAt: string }>("/api/v1/auth/invites", {
    method: "POST",
    body: JSON.stringify(expiresInDays === undefined ? {} : { expiresInDays }),
  });
}

/**
 * Cloud's operator console, reached through the dashboard server's read-only
 * proxy (see server.ts CLOUD_ALLOWLIST) rather than called directly — the
 * browser is never given Cloud's API key.
 *
 * Cloud being down surfaces here as an ApiError with reason
 * "cloud_unavailable" (the proxy's 503 passthrough) or "network" (the proxy
 * itself unreachable). Every page that calls these must treat that as a
 * degrade-to-"unavailable" case, not let it throw into a blank screen — Cloud
 * is expected to be down sometimes.
 */
export type CloudStatus = {
  tools?: { total?: number; healthy?: number; degraded?: number; offline?: number };
  users?: { total?: number };
  peers?: { total?: number; known?: number };
  node?: { id?: string; shortId?: string; did?: string };
};

export type CloudTrustCounts = {
  total?: number;
  trusted?: number;
  verified?: number;
  pending?: number;
  quarantined?: number;
  revoked?: number;
  expired?: number;
};

export type CloudTrust = { nodes: CloudTrustCounts; peers: CloudTrustCounts; updatedAt?: string };

export type CloudIdentity = { address?: string; shortId?: string; did?: string; publicKey?: string };

export type CloudAuditEvent = {
  subjectId?: string;
  timestamp?: string;
  metadata?: {
    action?: string;
    actor?: string;
    previousState?: string;
    nextState?: string;
    reason?: string;
  };
};

export type CloudTool = {
  id?: string;
  name: string;
  capabilities?: string[];
  health?: string;
  status?: string;
  publicUrl?: string;
  upstreamUrl?: string;
  lastHeartbeatAt?: string;
  registeredAt?: string;
};

export function cloudStatus() {
  return request<CloudStatus>("/api/cloud/status");
}

/** status.html's `?compact=trust` query, forwarded verbatim by the proxy. */
export function cloudTrust() {
  return request<{ trust?: CloudTrust }>("/api/cloud/status?compact=trust").then((r) => r.trust ?? null);
}

export function cloudIdentity() {
  return request<CloudIdentity>("/api/cloud/federation/identity");
}

export function cloudAudit() {
  return request<{ events?: CloudAuditEvent[] }>(
    "/api/cloud/audit?eventType=node-trust-action&kind=audit&limit=10",
  ).then((r) => r.events ?? []);
}

export function cloudTools() {
  return request<{ tools?: CloudTool[] }>("/api/cloud/tools").then((r) => r.tools ?? []);
}

/**
 * `view-users` in status.html (loadUsersView). Admin-only server-side (see
 * server.ts CLOUD_ALLOWLIST) — a non-admin caller gets ApiError("forbidden")
 * from a 403, which the page must render as an explained state, not swallow
 * into the generic "unavailable" case the other cloud views use.
 */
/** `view-federation` in status.html (loadFederationView). */
export type CloudPeer = {
  domain?: string;
  url?: string;
  trustLevel?: string;
  // Cloud's real FederationPeer carries `trust` as an object (a signed trust
  // record), not the display string status.html's loadFederationView assumed
  // — a pre-existing drift. Typed `unknown` so callers must coerce it to text
  // before rendering rather than handing an object straight to React.
  trust?: unknown;
  lastSeen?: string;
  address?: string;
  nodeAddress?: string;
};

export function cloudFederationPeers() {
  return request<{ peers?: CloudPeer[] }>("/api/cloud/federation/peers").then((r) => r.peers ?? []);
}

/** `view-api` in status.html (loadApiView). */
export type CloudEndpoint = { method: string; path: string; description?: string };

export function cloudEndpoints() {
  return request<{ routes?: CloudEndpoint[]; endpoints?: CloudEndpoint[] }>("/api/cloud/endpoints")
    .then((r) => r.routes ?? r.endpoints ?? []);
}
