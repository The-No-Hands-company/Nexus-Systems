export type AuthContractRoute = {
  method: "GET" | "POST" | "PUT" | "PATCH";
  path: string;
  access: "public" | "session-or-bearer" | "session-with-permission" | "founder-or-admin" | "internal";
  description: string;
};

export type NexusAuthContract = {
  protocol: "nexus-auth/1";
  service: {
    id: "nexus-auth";
    name: "Nexus Auth";
    surface: "platform-service";
    hostedPath: "/auth";
    auth: "Nexus Auth session or Bearer credential";
  };
  routes: readonly AuthContractRoute[];
};

const ROUTES: readonly AuthContractRoute[] = [
  { method: "GET", path: "/health", access: "public", description: "Auth service liveness" },
  { method: "GET", path: "/api/v1/auth/readiness", access: "public", description: "Identity and integration readiness" },
  { method: "GET", path: "/api/v1/auth/contract", access: "public", description: "Versioned platform-service contract" },
  { method: "GET", path: "/api/v1/auth/contracts/registration", access: "public", description: "Cloud registration payload" },
  { method: "POST", path: "/api/v1/auth/login", access: "public", description: "Create a hosted session" },
  { method: "POST", path: "/api/v1/auth/access-requests", access: "public", description: "Request account access" },
  { method: "POST", path: "/api/v1/auth/claim", access: "public", description: "Claim an approved account" },
  { method: "GET", path: "/api/v1/auth/me", access: "session-or-bearer", description: "Read the caller identity" },
  { method: "GET", path: "/api/v1/auth/trust", access: "session-or-bearer", description: "Validate the caller credential" },
  { method: "GET", path: "/api/v1/auth/preferences/catalog", access: "session-or-bearer", description: "Read self-scoped catalog preferences" },
  { method: "PUT", path: "/api/v1/auth/preferences/catalog", access: "session-or-bearer", description: "Save self-scoped catalog preferences" },
  { method: "GET", path: "/api/v1/auth/api-keys", access: "session-or-bearer", description: "List permitted API keys" },
  { method: "POST", path: "/api/v1/auth/api-keys", access: "session-with-permission", description: "Issue an API key" },
  { method: "POST", path: "/api/v1/auth/api-keys/:id/revoke", access: "session-with-permission", description: "Revoke an API key" },
  { method: "POST", path: "/api/v1/auth/tokens/issue", access: "session-with-permission", description: "Issue a service token" },
  { method: "POST", path: "/api/v1/auth/tokens/validate", access: "public", description: "Validate a service token" },
  { method: "GET", path: "/api/v1/auth/check", access: "session-or-bearer", description: "Return identity and permissions for services" },
  { method: "GET", path: "/api/v1/auth/oauth/jwks", access: "public", description: "OIDC signing keys" },
  { method: "GET", path: "/api/v1/auth/oauth/userinfo", access: "session-or-bearer", description: "OIDC user information" },
  { method: "GET", path: "/api/v1/auth/users", access: "founder-or-admin", description: "List hosted accounts" },
];

export function buildAuthContract(): NexusAuthContract {
  return {
    protocol: "nexus-auth/1",
    service: {
      id: "nexus-auth",
      name: "Nexus Auth",
      surface: "platform-service",
      hostedPath: "/auth",
      auth: "Nexus Auth session or Bearer credential",
    },
    routes: ROUTES,
  };
}
