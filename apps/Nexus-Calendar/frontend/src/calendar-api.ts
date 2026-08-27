export type EventAccess = "owner" | "editor" | "viewer";
export type EventPermission = "editor" | "viewer";

export type CalEvent = {
  id: string;
  title: string;
  description?: string;
  location?: string;
  startTime: string;
  endTime: string;
  allDay: boolean;
  recurrence?: string;
  createdAt: string;
  ownerSubject: string;
  access: EventAccess;
};

export type EventInput = Pick<CalEvent, "title" | "startTime" | "endTime"> & Partial<Pick<CalEvent, "description" | "location" | "allDay" | "recurrence">>;
export type EventShare = { eventId: string; subject: string; permission: EventPermission; createdBy: string; createdAt: string; updatedAt: string };
export type PublicEvent = Pick<CalEvent, "title" | "startTime" | "endTime" | "allDay"> & { location: string | null; description: string | null };
export type PublicShare = { token: string; publicPath: string };

export class CalendarApiError extends Error {
  constructor(readonly reason: string, readonly status: number) {
    super(reason);
    this.name = "CalendarApiError";
  }
}

export type CalendarApi = {
  listEvents(from: string, to: string): Promise<CalEvent[]>;
  getEvent(id: string): Promise<CalEvent>;
  createEvent(input: EventInput): Promise<CalEvent>;
  updateEvent(id: string, patch: Partial<EventInput>): Promise<CalEvent>;
  deleteEvent(id: string): Promise<void>;
  listShares(id: string): Promise<EventShare[]>;
  grantShare(id: string, subject: string, permission: EventPermission): Promise<EventShare>;
  revokeShare(id: string, subject: string): Promise<void>;
  createPublicLink(id: string): Promise<PublicShare>;
  revokePublicLink(id: string): Promise<void>;
  getPublicEvent(token: string): Promise<PublicEvent>;
};

function reasonFor(status: number, envelope: unknown): string {
  if (status === 400) return "validation";
  if (status === 401) return "unauthenticated";
  if (status === 403) return "forbidden";
  if (status === 404) return "not_found";
  if (status === 503) return "unavailable";
  if (envelope && typeof envelope === "object" && "error" in envelope && (envelope as { error: unknown }).error === "calendar_unavailable") return "unavailable";
  return "unavailable";
}

async function request<T>(fetcher: typeof fetch, base: string, path: string, init: RequestInit = {}): Promise<T> {
  let response: Response;
  try {
    const headers = new Headers(init.headers);
    headers.set("accept", "application/json");
    if (init.body) headers.set("content-type", "application/json");
    response = await fetcher(`${base}${path}`, {
      credentials: "same-origin",
      ...init,
      headers,
    });
  } catch {
    throw new CalendarApiError("offline", 0);
  }

  let body: unknown = undefined;
  try { body = await response.json(); } catch { /* a failed proxy can legally return a non-JSON response */ }
  if (!response.ok) throw new CalendarApiError(reasonFor(response.status, body), response.status);
  return body as T;
}

export function calendarApi(apiBase: string, fetcher: typeof fetch = fetch): CalendarApi {
  const eventPath = (id: string) => `/events/${encodeURIComponent(id)}`;
  return {
    async listEvents(from, to) {
      const data = await request<{ events?: unknown }>(fetcher, apiBase, `/events?from=${encodeURIComponent(from)}&to=${encodeURIComponent(to)}`);
      if (!Array.isArray(data.events)) throw new CalendarApiError("unavailable", 502);
      return data.events as CalEvent[];
    },
    getEvent: (id) => request(fetcher, apiBase, eventPath(id)),
    createEvent: (input) => request(fetcher, apiBase, "/events", { method: "POST", body: JSON.stringify(input) }),
    updateEvent: (id, patch) => request(fetcher, apiBase, eventPath(id), { method: "PATCH", body: JSON.stringify(patch) }),
    async deleteEvent(id) { await request(fetcher, apiBase, eventPath(id), { method: "DELETE" }); },
    async listShares(id) {
      const data = await request<{ shares?: unknown }>(fetcher, apiBase, `${eventPath(id)}/shares`);
      if (!Array.isArray(data.shares)) throw new CalendarApiError("unavailable", 502);
      return data.shares as EventShare[];
    },
    grantShare: (id, subject, permission) => request(fetcher, apiBase, `${eventPath(id)}/shares/${encodeURIComponent(subject)}`, { method: "PUT", body: JSON.stringify({ permission }) }),
    async revokeShare(id, subject) { await request(fetcher, apiBase, `${eventPath(id)}/shares/${encodeURIComponent(subject)}`, { method: "DELETE" }); },
    createPublicLink: (id) => request(fetcher, apiBase, `${eventPath(id)}/public-share`, { method: "POST" }),
    async revokePublicLink(id) { await request(fetcher, apiBase, `${eventPath(id)}/public-share`, { method: "DELETE" }); },
    getPublicEvent: (token) => request(fetcher, apiBase, `/public/${encodeURIComponent(token)}`),
  };
}
