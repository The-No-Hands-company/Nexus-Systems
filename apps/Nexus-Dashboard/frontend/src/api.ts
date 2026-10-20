import { normalizeCatalogEntry } from "./shell/catalog";

/**
 * Same-origin calls to the dashboard server.
 *
 * Most paths here are relative on purpose. The dashboard server proxies
 * /ipa/v1/auth/* to Nexus-Auth, so the browser never makes a cross-origin
 * request and never needs credentialed CORS for those calls.
 *
 * Identity and app catalog use the stable platform facade at api.tnhc.dev
 * (VITE_NEXUS_API_URL). Those calls are cross-origin and use
 * credentials: "include" so the session cookie is sent.
 */

export function platformApiOrigin(): string {
  return (
    (typeof import.meta !== "undefined" && (import.meta as { env?: { VITE_NEXUS_API_URL?: string } }).env?.VITE_NEXUS_API_URL) ||
    "https://api.tnhc.dev"
  ).replace(/\/+$/, "");
}

async function platformRequest<T>(path: string, init?: RequestInit): Promise<T> {
  const url = `${platformApiOrigin()}${path}`;
  let res: Response;
  try {
    res = await fetch(url, {
      ...init,
      credentials: "include",
      headers: { "content-type": "application/json", ...(init?.headers ?? {}) },
    });
  } catch {
    throw new ApiError("network", "Could not reach the server. Check your connection and try again.");
  }
  const body = await res.json().catch(() => ({}) as Record<string, unknown>);
  if (!res.ok) {
    const reason =
      typeof (body as { error?: unknown }).error === "string"
        ? (body as { error: string }).error
        : `http_${res.status}`;
    throw new ApiError(reason, reason);
  }
  return body as T;
}

export type CatalogShelf = "everyday" | "work" | "create" | "develop" | "operate" | "personal" | "explore";
export type CatalogAudience = "general" | "business" | "creator" | "developer" | "operations" | "power-user";
export type CatalogMaturity = "stable" | "preview" | "experimental";
export type CatalogVisibility = "default" | "discoverable" | "restricted" | "hidden";
export type CatalogReviewStatus = "approved" | "pending" | "deferred" | "rejected";
export type CatalogSurface = "user-app" | "admin-console" | "platform-service" | "developer-tool";
export type CatalogReleaseState = "staged" | "released" | "retired";

export type CatalogPreferences = {
  pinnedIds: string[];
  recentIds: string[];
  preferredShelf: CatalogShelf;
};

export type AppEntry = {
  id: string;
  name: string;
  description: string;
  /** The in-shell route — /chat, /mail, /cloud. Names the app, not how it is
   *  delivered, so moving an app between framed and shell-native never changes
   *  its URL. Mirrors the server's AppEntry in src/apps.ts.
   *
   *  Always present after listApps() normalises it, so components can rely on
   *  it; see WireAppEntry for what the server may actually send. */
  path: string;
  publicUrl?: string;
  delivery: "shell-native" | "proxied-app" | "framed" | "external";
  health: "healthy" | "offline";
  /** Optional for legacy callers; listApps() always fills these fields. */
  primaryShelf?: CatalogShelf;
  secondaryShelves?: CatalogShelf[];
  audiences?: CatalogAudience[];
  capabilities?: string[];
  maturity?: CatalogMaturity;
  visibility?: CatalogVisibility;
  reviewStatus?: CatalogReviewStatus;
  surface: CatalogSurface;
  releaseState: CatalogReleaseState;
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

export async function me(): Promise<Me | null> {
  try {
    const { user } = await platformRequest<{ user: Me }>("/api/v1/me");
    return user;
  } catch {
    // Not signed in is the common case here, not an error worth surfacing.
    return null;
  }
}

/**
 * Uses only an explicit in-shell path sent by the server.
 *
 * The static bundle and the server process are deployed independently — the
 * server serves `dist/` from disk, so a rebuilt frontend may temporarily talk
 * to an older server. A legacy relative `url` is an explicit route. Missing
 * paths remain empty so the catalog projection cannot invent a destination.
 */
type WireAppEntry = {
  id: string;
  name?: string;
  description?: string;
  path?: string;
  publicUrl?: string;
  /** Compatibility field from Dashboard servers that predate explicit delivery. */
  url?: string;
  delivery?: AppEntry["delivery"];
  health?: AppEntry["health"];
  primaryShelf?: unknown;
  secondaryShelves?: unknown;
  audiences?: unknown;
  capabilities?: unknown;
  maturity?: unknown;
  visibility?: unknown;
  reviewStatus?: unknown;
  surface?: unknown;
  releaseState?: unknown;
};

function pathFor(app: WireAppEntry): string {
  if (app.path) return app.path;
  if (app.url?.startsWith("/")) return app.url;
  // A fabricated slug is not proof that a released app has a usable route.
  return "";
}

export async function listApps(): Promise<AppEntry[]> {
  const { apps } = await platformRequest<{ apps: WireAppEntry[] }>("/api/v1/apps");
  // The catalog helper is the single frontend normalization boundary. Older
  // Dashboard servers may omit every placement field, so components must not
  // inspect the wire response directly.
  // Normalise once, here, so no component has to care whether the server that
  // answered is older than the bundle asking.
  return apps.map(({ url, ...a }) => {
    const path = pathFor({ ...a, url });
    const publicUrl = a.publicUrl ?? (url?.startsWith("/") ? undefined : url);
    const delivery = a.delivery ?? (url?.startsWith("/") ? "shell-native" : "framed");
    return normalizeCatalogEntry({ ...a, path, ...(publicUrl ? { publicUrl } : {}), delivery });
  });
}

export function getCatalogPreferences() {
  return request<{ preferences: CatalogPreferences }>("/ipa/v1/auth/preferences/catalog")
    .then((response) => response.preferences);
}

export function saveCatalogPreferences(preferences: CatalogPreferences) {
  return request<{ preferences: CatalogPreferences }>("/ipa/v1/auth/preferences/catalog", {
    method: "PUT",
    body: JSON.stringify(preferences),
  }).then((response) => response.preferences);
}

export type Session = {
  id: string;
  deviceId: string;
  createdAt: string;
  expiresAt: string;
  current: boolean;
};

export type ActivityEvent = { event: string; deviceId: string | null; at: string };

export function myActivity() {
  return request<{ events: ActivityEvent[] }>("/ipa/v1/auth/activity").then((r) => r.events);
}

export function listSessions() {
  return request<{ sessions: Session[] }>("/ipa/v1/auth/sessions").then((r) => r.sessions);
}

export function revokeSession(id: string) {
  return request<{ success: boolean }>(`/ipa/v1/auth/sessions/${encodeURIComponent(id)}/revoke`, {
    method: "POST",
  });
}

export function remainingRecoveryCodes() {
  return request<{ remaining: number }>("/ipa/v1/auth/recovery-codes").then((r) => r.remaining);
}

export function regenerateRecoveryCodes() {
  return request<{ recoveryCodes: string[] }>("/ipa/v1/auth/recovery-codes/regenerate", {
    method: "POST",
  }).then((r) => r.recoveryCodes);
}

/**
 * Always posts to the caller's own id. Self-service is authorised by the
 * current password, not by an admin permission.
 */
export function changePassword(userId: string, currentPassword: string, newPassword: string) {
  return request<{ success: boolean }>(
    `/ipa/v1/auth/users/${encodeURIComponent(userId)}/password`,
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
  return request<{ requests: AccessRequest[] }>("/ipa/v1/auth/access-requests")
    .then((r) => r.requests);
}

/** What happened to an email Auth asked Nexus-Email to send. */
export type MailDelivery = "sent" | "failed" | "rejected" | "disabled" | "not_requested";

export function decideAccessRequest(id: string, decision: "approve" | "reject") {
  return request<{ user: unknown; notice?: MailDelivery }>(
    `/ipa/v1/auth/access-requests/${encodeURIComponent(id)}/${decision}`,
    { method: "POST", body: JSON.stringify({}) },
  );
}

export type CreatedInvite = {
  id: string;
  code: string;
  expiresAt: string;
  link?: string;
  delivery?: MailDelivery;
};

/** With an email, Auth binds the invite to that address and emails it. */
export function createInvite(input: { email?: string } = {}) {
  return request<CreatedInvite>("/ipa/v1/auth/invites", {
    method: "POST",
    body: JSON.stringify(input.email ? { email: input.email } : {}),
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
 * The real shape of Cloud's `/ipa/v1/status`.
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

/** The Systems API wraps the normalized status counters in `status`. */
export type CloudStatusResponse = { status: CloudStatus };

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
  return request<CloudStatusResponse>("/ipa/cloud/status").then((r) => r.status);
}

/** status.html's `?compact=trust` query, forwarded verbatim by the proxy. */
export function cloudTrust() {
  return request<{ trust?: CloudTrust }>("/ipa/cloud/status?compact=trust").then((r) => r.trust ?? null);
}

export function cloudIdentity() {
  return request<CloudIdentity>("/ipa/cloud/federation/identity");
}

export function cloudAudit() {
  return request<{ events?: CloudAuditEvent[] }>(
    "/ipa/cloud/audit?eventType=node-trust-action&kind=audit&limit=10",
  ).then((r) => r.events ?? []);
}

export function cloudTools() {
  return request<{ tools?: CloudTool[] }>("/ipa/cloud/tools").then((r) => r.tools ?? []);
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
  return request<{ peers?: CloudPeer[] }>("/ipa/cloud/federation/peers").then((r) => r.peers ?? []);
}

/** `view-api` in status.html (loadApiView). */
export type CloudEndpoint = { method: string; path: string; description?: string };

export function cloudEndpoints() {
  return request<{ routes?: CloudEndpoint[]; endpoints?: CloudEndpoint[] }>("/ipa/cloud/endpoints")
    .then((r) => r.routes ?? r.endpoints ?? []);
}

// ── Development helper tools (dhts/) ───────────────────────────────────────
//
// Founder-only, served by the dashboard server from its curated registry.
// Run commands live server-side; the client can only name a tool.

export type DevTool = {
  id: string;
  dir: string;
  name: string;
  description: string;
  command: string;
  runnable?: { cmd: string; args: string[] };
};

export type DevToolsResponse = { root: string; exists: boolean; tools: DevTool[] };

export type DevToolRunResult = {
  ok: boolean;
  exitCode: number | null;
  output: string;
  error?: string;
};

export function listDevTools() {
  return request<DevToolsResponse>("/ipa/dev/tools");
}

export function runDevTool(id: string) {
  return request<DevToolRunResult>("/ipa/dev/tools/run", {
    method: "POST",
    body: JSON.stringify({ id }),
  });
}


export type ServiceStatus = {
  name: string;
  description: string;
  healthy: boolean;
  latencyMs: number | null;
  detail?: string;
};

export function listServiceHealth() {
  return request<{ services: ServiceStatus[] }>("/ipa/dev/services");
}

// ── User management (founder-only) ─────────────────────────────────────────

export type ManagedUser = {
  id: string;
  username: string;
  email: string;
  role: string;
  status: string;
  createdAt?: string;
};

export function listUsers() {
  return request<{ users: ManagedUser[] }>("/ipa/v1/auth/users").then((r) => r.users ?? []);
}

export function updateUserRole(userId: string, role: string) {
  return request<{ user: ManagedUser }>(`/ipa/v1/auth/users/${encodeURIComponent(userId)}`, {
    method: "PATCH",
    body: JSON.stringify({ role }),
  });
}

export function suspendUser(userId: string) {
  return request<{ user: ManagedUser }>(`/ipa/v1/auth/users/${encodeURIComponent(userId)}`, {
    method: "PATCH",
    body: JSON.stringify({ status: "suspended" }),
  });
}

export function activateUser(userId: string) {
  return request<{ user: ManagedUser }>(`/ipa/v1/auth/users/${encodeURIComponent(userId)}`, {
    method: "PATCH",
    body: JSON.stringify({ status: "active" }),
  });
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
  return platformRequest<MailFolder[]>("/api/v1/mail/folders");
}

export function listMailMessages(folderId: string) {
  return platformRequest<MailSummary[]>(`/api/v1/mail/folders/${encodeURIComponent(folderId)}/messages`);
}

export function readMailMessage(id: string) {
  return platformRequest<MailMessage>(`/api/v1/mail/messages/${encodeURIComponent(id)}`);
}

export function markMailSeen(id: string, seen: boolean) {
  return platformRequest<void>(`/api/v1/mail/messages/${encodeURIComponent(id)}/seen`, {
    method: "POST",
    body: JSON.stringify({ seen }),
  });
}

export function threadMessages(threadId: string) {
  return platformRequest<MailSummary[]>(`/api/v1/mail/threads/${encodeURIComponent(threadId)}/messages`);
}

/// The URL an attachment downloads from. Not fetched via platformRequest: the
/// response is bytes, not JSON, and the browser handles the download itself.
export function attachmentUrl(messageId: string, index: number) {
  return `${platformApiOrigin()}/api/v1/mail/messages/${encodeURIComponent(messageId)}/attachments/${index}`;
}

export function searchMail(q: string) {
  return platformRequest<MailSummary[]>(`/api/v1/mail/search?q=${encodeURIComponent(q)}`);
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
  return platformRequest<{ outcomes: SendOutcome[] }>("/api/v1/mail/messages", {
    method: "POST",
    body: JSON.stringify(body),
  });
}

// ── Calendar ─────────────────────────────────────────────────────────────────
//
// All calendar routes go through the platform facade (api.tnhc.dev), which
// resolves identity from the session cookie and forwards x-nexus-subject +
// x-nexus-dashboard-secret to Nexus-Calendar. The browser never supplies
// either header — the facade always overwrites them.

export type CalendarEvent = {
  id: string;
  title: string;
  startTime: string;
  endTime?: string;
  allDay?: boolean;
  description?: string;
  location?: string;
};

export type CalendarEventInput = {
  title: string;
  startTime: string;
  endTime?: string;
  allDay?: boolean;
  description?: string;
  location?: string;
};

export function listCalendarEvents(from: string, to: string) {
  return platformRequest<{ events: CalendarEvent[] }>(
    `/api/v1/calendar/events?from=${encodeURIComponent(from)}&to=${encodeURIComponent(to)}`,
  ).then((r) => {
    if (!Array.isArray(r.events)) throw new Error("malformed_response");
    return r.events;
  });
}

export function getCalendarEvent(id: string) {
  return platformRequest<{ event: CalendarEvent }>(
    `/api/v1/calendar/events/${encodeURIComponent(id)}`,
  ).then((r) => r.event);
}

export function createCalendarEvent(input: CalendarEventInput) {
  return platformRequest<{ event: CalendarEvent }>("/api/v1/calendar/events", {
    method: "POST",
    body: JSON.stringify(input),
  }).then((r) => r.event);
}

export function updateCalendarEvent(id: string, patch: Partial<CalendarEventInput>) {
  return platformRequest<{ event: CalendarEvent }>(
    `/api/v1/calendar/events/${encodeURIComponent(id)}`,
    { method: "PATCH", body: JSON.stringify(patch) },
  ).then((r) => r.event);
}

export function deleteCalendarEvent(id: string) {
  return platformRequest<void>(`/api/v1/calendar/events/${encodeURIComponent(id)}`, {
    method: "DELETE",
  });
}

export function getPublicCalendarEvent(token: string) {
  return platformRequest<{ event: CalendarEvent }>(
    `/api/v1/calendar/public/${encodeURIComponent(token)}`,
  ).then((r) => r.event);
}

/** A filed issue, as GitHub returned it. */
export type FiledIssue = { number: number; url: string };

/**
 * File an issue from inside the dashboard.
 *
 * The reporter is identified server-side from the session — nothing about who
 * is reporting travels in this body, so it cannot be spoofed by editing the
 * request.
 */
export async function reportIssue(input: {
  title: string;
  body: string;
  app?: string;
  url?: string;
}): Promise<FiledIssue> {
  return request<FiledIssue>("/ipa/issues", {
    method: "POST",
    body: JSON.stringify(input),
  });
}

/** A system notification, as Hosting recorded it. */
export type Notification = {
  id: number;
  event: string;
  title: string;
  body: string | null;
  href: string | null;
  readAt: string | null;
  createdAt: string;
};

/**
 * The signed-in user's notifications.
 *
 * Proxied through this origin to Nexus-Hosting, which owns the events. All
 * three calls are scoped server-side to the caller's session — nothing here
 * identifies a user, so nothing here can ask for somebody else's.
 */
export function listNotifications(unreadOnly = false) {
  return request<{ notifications: Notification[] }>(
    `/ipa/notifications${unreadOnly ? "?unread=true" : ""}`,
  ).then((r) => r.notifications);
}

export function unreadNotificationCount() {
  return request<{ unread: number }>("/ipa/notifications/unread-count").then((r) => r.unread);
}

export function markNotificationRead(id: number) {
  return request<{ ok: true }>(`/ipa/notifications/${id}/read`, { method: "POST" });
}

export function markAllNotificationsRead() {
  return request<{ marked: number }>("/ipa/notifications/read-all", { method: "POST" });
}
