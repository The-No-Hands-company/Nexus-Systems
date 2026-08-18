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
/**
 * The real shape of Cloud's `/api/v1/status`.
 *
 * status.html read `status.tools.total`, `status.users.total`,
 * `status.peers.total` and `status.node.*`. None of those have ever existed on
 * this response — the counters are flat and differently named — so the old
 * console rendered zeros for all of them while Cloud was reporting 86 tools.
 * Transcribed from the live response rather than from the code that consumed
 * it.
 *
 * There is no user count here, and there should not be: accounts belong to
 * Nexus-Auth now (Cloud's own POST /api/v1/users answers 410 saying so).
 */
export type CloudStatus = {
  version?: string;
  mode?: string;
  toolCount?: number;
  exposedToolCount?: number;
  healthyToolCount?: number;
  publicUrlCount?: number;
  addressCount?: number;
  activeExposureCount?: number;
  domainCount?: number;
  verifiedDomainCount?: number;
  integrationStatus?: string;
  failedIntegrationCount?: number;
  /** Peer/node counts live under trust, not at the top level. */
  trust?: { nodes?: CloudTrustCounts; peers?: CloudTrustCounts };
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

/**
 * `exampleAddress` is deliberately not renamed to `address`.
 *
 * It is `@alice:<shortId>` — an illustration of this node's naming scheme, not
 * an address belonging to anyone. status.html displayed it under an "Address"
 * label, which reads as "this node is @alice". Keeping the upstream name means
 * the view cannot casually mislabel it again.
 */
export type CloudIdentity = {
  did?: string;
  shortId?: string;
  publicKey?: string;
  namingScheme?: string;
  exampleAddress?: string;
  addressNote?: string;
};

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

// ── Mail ────────────────────────────────────────────────────────────────────
//
// Everything goes through the dashboard's /api/mail proxy, which attaches the
// caller's identity server-side. The browser never says whose mailbox it wants
// and could not be trusted to.

export type MailFolder = { id: string; name: string; kind: string };

export type MailSummary = {
  id: string;
  thread_id: string;
  subject: string | null;
  from: string;
  received_at: string;
  seen: boolean;
  flagged: boolean;
  snippet: string | null;
};

export type MailAttachment = {
  /// Position in the MIME tree. Filenames are sender-chosen and not unique,
  /// so they cannot be used to identify which attachment to fetch.
  index: number;
  filename: string;
  mime_type: string;
  size: number;
};

export type MailMessage = {
  id: string;
  thread_id: string | null;
  subject: string | null;
  from: string;
  to: string | null;
  date: string | null;
  text: string;
  /// Already sanitised server-side. Still rendered in a sandboxed frame — one
  /// layer of defence against a stranger's markup is not enough.
  html: string | null;
  /// Remote content was withheld, so the reader can be offered the choice.
  blocked_remote: boolean;
  attachments: MailAttachment[];
};

export type SendOutcome = { recipient: string; status: string; reason: string | null };

export function listMailFolders() {
  return request<MailFolder[]>("/api/mail/folders");
}

export function listMailMessages(folderId: string) {
  return request<MailSummary[]>(`/api/mail/folders/${encodeURIComponent(folderId)}/messages`);
}

export function readMailMessage(id: string) {
  return request<MailMessage>(`/api/mail/messages/${encodeURIComponent(id)}`);
}

export function markMailSeen(id: string, seen: boolean) {
  return request<void>(`/api/mail/messages/${encodeURIComponent(id)}/seen`, {
    method: "POST",
    body: JSON.stringify({ seen }),
  });
}

export function threadMessages(threadId: string) {
  return request<MailSummary[]>(`/api/mail/threads/${encodeURIComponent(threadId)}/messages`);
}

/// The URL an attachment downloads from. Not fetched through `request`: the
/// response is bytes, not JSON, and the browser handles the download itself.
export function attachmentUrl(messageId: string, index: number) {
  return `/api/mail/messages/${encodeURIComponent(messageId)}/attachments/${index}`;
}

export function searchMail(q: string) {
  return request<MailSummary[]>(`/api/mail/search?q=${encodeURIComponent(q)}`);
}

export type OutgoingAttachment = {
  filename: string;
  mime_type: string;
  /// Base64. JSON cannot carry raw bytes, so the composer encodes and the
  /// server decodes; the size limit is applied to the decoded length.
  data: string;
};

export function sendMail(body: {
  to: string[];
  cc?: string[];
  subject: string;
  text: string;
  in_reply_to?: string;
  references?: string[];
  attachments?: OutgoingAttachment[];
}) {
  return request<{ outcomes: SendOutcome[] }>("/api/mail/messages", {
    method: "POST",
    body: JSON.stringify(body),
  });
}
