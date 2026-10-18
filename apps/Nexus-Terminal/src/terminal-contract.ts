export type TerminalContractRoute = {
  method: "GET" | "WebSocket";
  path: string;
  access: "public" | "founder-or-admin" | "internal";
  description: string;
};

export type NexusTerminalContract = {
  protocol: "nexus-terminal/1";
  service: {
    id: "nexus-terminal";
    name: "Nexus Terminal";
    auth: "Nexus-Auth session cookie or Bearer credential";
    enabledByDefault: false;
  };
  routes: readonly TerminalContractRoute[];
};

const ROUTES: readonly TerminalContractRoute[] = [
  { method: "GET", path: "/health", access: "public", description: "Liveness and shell state summary" },
  { method: "GET", path: "/api/v1/status", access: "public", description: "Terminal service capabilities" },
  { method: "GET", path: "/api/v1/terminal/contract", access: "public", description: "Guarded Terminal contract" },
  { method: "WebSocket", path: "/api/v1/terminal/attach", access: "founder-or-admin", description: "Attach an audited host shell when explicitly enabled" },
  { method: "GET", path: "/api/v1/terminal/audit", access: "founder-or-admin", description: "Read recent audited shell sessions" },
  { method: "GET", path: "/api/v1/terminal/sessions", access: "internal", description: "Internal session records" },
  { method: "GET", path: "/api/v1/terminal/commands", access: "internal", description: "Internal command records" },
];

export function buildTerminalContract(): NexusTerminalContract {
  return {
    protocol: "nexus-terminal/1",
    service: {
      id: "nexus-terminal",
      name: "Nexus Terminal",
      auth: "Nexus-Auth session cookie or Bearer credential",
      enabledByDefault: false,
    },
    routes: ROUTES,
  };
}
