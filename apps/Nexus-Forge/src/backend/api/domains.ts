interface DomainStub {
  name: string;
  description: string;
  capabilities: string[];
  endpoints: string[];
  state: "planned" | "stubbed" | "in-progress" | "ready";
}

const domainStubs: DomainStub[] = [
  {
    name: "security",
    description: "Application and repository security controls.",
    capabilities: ["secret-scanning", "dependency-scan", "codeowners", "signing-policies"],
    endpoints: ["/api/security/overview", "/api/security/alerts", "/api/security/policies"],
    state: "stubbed",
  },
  {
    name: "governance",
    description: "Policy-as-code and enterprise approval controls.",
    capabilities: ["approval-rules", "rbac", "retention", "compliance-reports"],
    endpoints: ["/api/governance/policies", "/api/governance/approvals", "/api/governance/roles"],
    state: "stubbed",
  },
  {
    name: "integrations",
    description: "Third-party and identity integrations.",
    capabilities: ["slack", "discord", "jira", "saml-sso", "ldap"],
    endpoints: ["/api/integrations/providers", "/api/integrations/config", "/api/integrations/events"],
    state: "stubbed",
  },
  {
    name: "notifications",
    description: "Delivery channels and notification routing.",
    capabilities: ["email", "inbox", "webhooks", "push"],
    endpoints: ["/api/notifications/channels", "/api/notifications/preferences"],
    state: "stubbed",
  },
  {
    name: "packages",
    description: "Multi-ecosystem package hosting.",
    capabilities: ["npm", "container", "maven", "pypi"],
    endpoints: ["/api/packages/registries", "/api/packages/artifacts", "/api/packages/policies"],
    state: "stubbed",
  },
  {
    name: "federation",
    description: "Cross-node replication and trust mesh.",
    capabilities: ["peer-discovery", "replication", "attestations", "failover"],
    endpoints: ["/api/federation/peers", "/api/federation/replicas", "/api/federation/trust"],
    state: "stubbed",
  },
  {
    name: "observability",
    description: "Metrics, traces, and debugging surfaces.",
    capabilities: ["metrics", "trace-explorer", "log-search", "alerts"],
    endpoints: ["/api/observability/metrics", "/api/observability/traces", "/api/observability/logs"],
    state: "stubbed",
  },
  {
    name: "enterprise",
    description: "Multi-tenant and enterprise administration.",
    capabilities: ["tenants", "legal-hold", "support-bundles", "audit-export"],
    endpoints: ["/api/enterprise/tenants", "/api/enterprise/legal-holds", "/api/enterprise/support"],
    state: "stubbed",
  },
];

export function registerDomainRoutes(app: any) {
  app.get("/api/domains", async () => {
    return {
      domains: domainStubs,
      total: domainStubs.length,
    };
  });

  app.get("/api/domains/:name", async ({ params }: any) => {
    const domainName = String(params.name || "").toLowerCase();
    const domain = domainStubs.find((item) => item.name === domainName);
    if (!domain) return { error: "Domain not found", status: 404 };
    return {
      domain,
      workbench: {
        status: "stubbed",
        nextMilestones: [
          "Define storage schema",
          "Create service layer",
          "Wire UI interactions",
          "Add integration tests",
        ],
      },
    };
  });

  app.get("/api/security/overview", async () => ({
    score: 72,
    scanners: ["secret-scan", "dependency-scan", "sast"],
    findings: 0,
    note: "Security overview is stubbed.",
  }));

  app.get("/api/security/alerts", async () => ({ alerts: [], note: "Security alert feed is stubbed." }));
  app.get("/api/security/policies", async () => ({ policies: [], note: "Security policy engine is stubbed." }));

  app.get("/api/governance/policies", async () => ({ policies: [], note: "Policy-as-code is stubbed." }));
  app.get("/api/governance/approvals", async () => ({ approvals: [], note: "Approval policy stubs enabled." }));
  app.get("/api/governance/roles", async () => ({ roles: [], note: "Advanced RBAC is stubbed." }));

  app.get("/api/integrations/providers", async () => ({
    providers: ["slack", "discord", "jira", "saml", "ldap"],
    configured: [],
    note: "Integrations are stubbed.",
  }));
  app.get("/api/integrations/config", async () => ({ settings: {}, note: "Integration config is stubbed." }));
  app.get("/api/integrations/events", async () => ({ events: [], note: "Outbound integration events are stubbed." }));

  app.get("/api/notifications/channels", async () => ({ channels: ["email", "webhook", "inbox", "push"] }));
  app.get("/api/notifications/preferences", async () => ({ preferences: {}, note: "Notification preferences are stubbed." }));

  app.get("/api/packages/registries", async () => ({ registries: ["npm", "container", "maven", "pypi"] }));
  app.get("/api/packages/artifacts", async () => ({ artifacts: [], note: "Artifact browser is stubbed." }));
  app.get("/api/packages/policies", async () => ({ policies: [], note: "Package policies are stubbed." }));

  app.get("/api/federation/peers", async () => ({ peers: [], note: "Peer registry is stubbed." }));
  app.get("/api/federation/replicas", async () => ({ replicas: [], note: "Replica topology is stubbed." }));
  app.get("/api/federation/trust", async () => ({ attestations: [], note: "Trust graph is stubbed." }));

  app.get("/api/observability/metrics", async () => ({
    metrics: [
      { name: "request_rate", value: 0 },
      { name: "error_rate", value: 0 },
      { name: "agent_task_duration_ms", value: 0 },
    ],
  }));
  app.get("/api/observability/traces", async () => ({ traces: [], note: "Trace explorer is stubbed." }));
  app.get("/api/observability/logs", async () => ({ logs: [], note: "Centralized log search is stubbed." }));

  app.get("/api/enterprise/tenants", async () => ({ tenants: [], note: "Multi-tenant management is stubbed." }));
  app.get("/api/enterprise/legal-holds", async () => ({ holds: [], note: "Legal hold workflows are stubbed." }));
  app.get("/api/enterprise/support", async () => ({ cases: [], note: "Support tooling is stubbed." }));
}
