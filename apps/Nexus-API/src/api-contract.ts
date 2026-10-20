export type ApiContractRoute = {
  method: "GET" | "POST" | "DELETE" | "PATCH" | "PUT";
  path: string;
  access: "public" | "session" | "operator";
  owner: "nexus-api" | "nexus-auth" | "nexus-cloud" | "nexus-email" | "nexus-calendar";
  description: string;
};

export type NexusApiContract = {
  protocol: "nexus-api/1";
  service: {
    id: "nexus-api";
    name: "Nexus API";
    version: string;
    baseUrl: string;
    auth: "Bearer token or hosted session";
  };
  routes: readonly ApiContractRoute[];
};

const ROUTES: readonly ApiContractRoute[] = [
  {
    method: "GET",
    path: "/health",
    access: "public",
    owner: "nexus-api",
    description: "Liveness and gateway health",
  },
  {
    method: "GET",
    path: "/api/v1/contract",
    access: "public",
    owner: "nexus-api",
    description: "Versioned Nexus API contract",
  },
  {
    method: "GET",
    path: "/api/v1/openapi.json",
    access: "public",
    owner: "nexus-api",
    description: "OpenAPI description for the stable platform facade",
  },
  {
    method: "GET",
    path: "/api/v1/me",
    access: "session",
    owner: "nexus-auth",
    description: "Read the authenticated platform caller",
  },
  {
    method: "GET",
    path: "/api/v1/apps",
    access: "session",
    owner: "nexus-cloud",
    description: "Read released app metadata for the caller",
  },
  {
    method: "GET",
    path: "/api/v1/catalog",
    access: "session",
    owner: "nexus-cloud",
    description: "Compatibility alias for the released app catalog",
  },
  {
    method: "GET",
    path: "/api/v1/calendar/events",
    access: "session",
    owner: "nexus-calendar",
    description: "List the caller's events in a date range",
  },
  {
    method: "POST",
    path: "/api/v1/calendar/events",
    access: "session",
    owner: "nexus-calendar",
    description: "Create an event for the caller",
  },
  {
    method: "GET",
    path: "/api/v1/calendar/events/:id",
    access: "session",
    owner: "nexus-calendar",
    description: "Read one caller-owned event",
  },
  {
    method: "PATCH",
    path: "/api/v1/calendar/events/:id",
    access: "session",
    owner: "nexus-calendar",
    description: "Update one caller-owned event",
  },
  {
    method: "DELETE",
    path: "/api/v1/calendar/events/:id",
    access: "session",
    owner: "nexus-calendar",
    description: "Delete one caller-owned event",
  },
  {
    method: "GET",
    path: "/api/v1/calendar/public/:token",
    access: "public",
    owner: "nexus-calendar",
    description:
      "Read a publicly shared event by its share token — the token is the access credential",
  },
  {
    method: "GET",
    path: "/api/v1/mail/folders",
    access: "session",
    owner: "nexus-email",
    description: "List the caller's mail folders",
  },
  {
    method: "GET",
    path: "/api/v1/mail/folders/:id/messages",
    access: "session",
    owner: "nexus-email",
    description: "List messages in one caller-owned folder",
  },
  {
    method: "GET",
    path: "/api/v1/mail/messages/:id",
    access: "session",
    owner: "nexus-email",
    description: "Read one caller-owned message",
  },
  {
    method: "POST",
    path: "/api/v1/mail/messages/:id/seen",
    access: "session",
    owner: "nexus-email",
    description: "Mark one caller-owned message read or unread",
  },
  {
    method: "POST",
    path: "/api/v1/mail/messages",
    access: "session",
    owner: "nexus-email",
    description: "Send a message as the caller",
  },
  {
    method: "GET",
    path: "/api/v1/mail/search",
    access: "session",
    owner: "nexus-email",
    description: "Search the caller's mailbox",
  },
  {
    method: "GET",
    path: "/api/v1/mail/threads/:id/messages",
    access: "session",
    owner: "nexus-email",
    description: "Read messages in one caller-owned thread",
  },
  {
    method: "GET",
    path: "/api/v1/mail/messages/:id/attachments/:index",
    access: "session",
    owner: "nexus-email",
    description: "Download one caller-owned attachment",
  },
  {
    method: "GET",
    path: "/api/v1/gateway/status",
    access: "public",
    owner: "nexus-api",
    description: "Gateway counters and uptime",
  },
  {
    method: "GET",
    path: "/api/v1/gateway/routes",
    access: "operator",
    owner: "nexus-api",
    description: "List configured compatibility proxy routes",
  },
  {
    method: "POST",
    path: "/api/v1/gateway/routes",
    access: "operator",
    owner: "nexus-api",
    description: "Create or replace a compatibility proxy route",
  },
  {
    method: "DELETE",
    path: "/api/v1/gateway/routes/:method/:path",
    access: "operator",
    owner: "nexus-api",
    description: "Remove a compatibility proxy route",
  },
];

export function buildApiContract(baseUrl: string): NexusApiContract {
  return {
    protocol: "nexus-api/1",
    service: {
      id: "nexus-api",
      name: "Nexus API",
      version: process.env.NEXUS_API_VERSION?.trim() || "1.0.0",
      baseUrl,
      auth: "Bearer token or hosted session",
    },
    routes: ROUTES,
  };
}
