/**
 * The first Nexus AI knowledge source is a small, versioned contract.
 *
 * It intentionally contains facts and provenance, not model output. A future
 * retrieval layer can index these documents, while callers can still show the
 * source and trust level beside an answer.
 */

export type KnowledgeDocument = {
  id: string;
  title: string;
  kind: "platform" | "app";
  summary: string;
  content: string;
  tags: string[];
  trust: "contract" | "approved-catalog";
  source: {
    kind: "architecture" | "catalog";
    ref: string;
    revision: string;
  };
};

export type KnowledgeCatalogApp = {
  id: string;
  name: string;
  description: string;
  path?: string;
  publicUrl?: string;
  primaryShelf: string;
  secondaryShelves: string[];
  audiences: string[];
  capabilities: string[];
  maturity: string;
  visibility: string;
  reviewStatus: string;
  health: string;
};

export type KnowledgeResponse = {
  protocol: "nexus-knowledge/1";
  status: "ready" | "degraded";
  generatedAt: string;
  source: {
    status: "ready" | "degraded";
    cloudUrl: string;
    catalogEndpoint: string;
  };
  documents: KnowledgeDocument[];
};

export type KnowledgeBuildOptions = {
  generatedAt: string;
  status: "ready" | "degraded";
  cloudUrl: string;
};

const ARCHITECTURE_SOURCE = {
  kind: "architecture" as const,
  ref: "docs/architecture/nexus-platform-surfaces.md",
  revision: "v1",
};

const PLATFORM_DOCUMENTS: KnowledgeDocument[] = [
  {
    id: "platform-auth",
    title: "Nexus Auth",
    kind: "platform",
    summary: "Identity, authentication, SSO, sessions, and role checks for the hosted platform.",
    content:
      "Nexus Auth is the platform service to authenticate users and provide authentication, login, SSO, identity, sessions, and role checks. It is a service used by the platform rather than a user app in the launcher. Self-hosted nodes may run it as a standalone module.",
    tags: ["auth", "authenticate", "authentication", "login", "sso", "identity", "platform-service"],
    trust: "contract",
    source: ARCHITECTURE_SOURCE,
  },
  {
    id: "platform-cloud",
    title: "Nexus Cloud",
    kind: "platform",
    summary: "Hosted control plane and administrative console for the ecosystem.",
    content:
      "Nexus Cloud is the hosted control plane and admin console. It owns registry, catalog approval, topology, federation coordination, service discovery, and platform operations. The hosted route is /cloud; it is an operator surface, not a general-purpose user app.",
    tags: ["cloud", "control-plane", "admin", "catalog", "topology", "federation"],
    trust: "contract",
    source: ARCHITECTURE_SOURCE,
  },
  {
    id: "platform-chat",
    title: "Nexus Chat",
    kind: "platform",
    summary: "Canonical hosted user messaging application.",
    content:
      "Nexus Chat is the canonical user messaging app at /chat. /nexus is a compatibility alias for older links and does not identify a second product. Its implementation lives in apps/Nexus.",
    tags: ["chat", "messaging", "user-app", "route", "nexus-alias"],
    trust: "contract",
    source: ARCHITECTURE_SOURCE,
  },
  {
    id: "platform-api",
    title: "Nexus API",
    kind: "platform",
    summary: "Versioned gateway and integration contract for ecosystem clients.",
    content:
      "Nexus API is the gateway and integration contract for clients and services. It provides versioned /api/v1 routes, authentication passthrough, service routing, and discovery contracts. It is an API surface rather than a launcher app for everyday users.",
    tags: ["api", "gateway", "integration", "api-v1", "developer"],
    trust: "contract",
    source: ARCHITECTURE_SOURCE,
  },
  {
    id: "platform-ai",
    title: "Nexus AI Hub",
    kind: "platform",
    summary: "Ecosystem-grounded AI orchestration and retrieval surface.",
    content:
      "Nexus AI Hub starts with approved Nexus Systems and TNHC ecosystem knowledge. Its initial retrieval source is the versioned knowledge contract and the approved Cloud catalog. Model providers can be added later without replacing the ecosystem source of truth.",
    tags: ["ai", "rag", "knowledge", "ecosystem", "providers"],
    trust: "contract",
    source: ARCHITECTURE_SOURCE,
  },
  {
    id: "platform-terminal",
    title: "Nexus Terminal",
    kind: "platform",
    summary: "Privileged audited shell for authorized hosted administrators and developers.",
    content:
      "Nexus Terminal is the browser-facing audited shell at /terminal. It is a developer and operations tool with explicit authorization, loopback service boundaries, session limits, and command audit logging. It is not an everyday user app.",
    tags: ["terminal", "cli", "developer", "operations", "audit", "privileged"],
    trust: "contract",
    source: ARCHITECTURE_SOURCE,
  },
];

function catalogDocument(app: KnowledgeCatalogApp): KnowledgeDocument {
  const path = app.path || `/${app.id.replace(/^nexus-/, "")}`;
  const shelfText = [app.primaryShelf, ...app.secondaryShelves].join(", ");
  const audienceText = app.audiences.join(", ") || "unspecified";
  const capabilityText = app.capabilities.join(", ") || "none listed";
  return {
    id: `app-${app.id}`,
    title: app.name,
    kind: "app",
    summary: app.description,
    content: `${app.name} is a ${app.reviewStatus} catalog entry with ${app.maturity} maturity and ${app.health} health. It opens at ${path}. Its primary shelf and secondary shelves are ${shelfText}. Its audiences are ${audienceText}; capabilities are ${capabilityText}.`,
    tags: [app.id, path, app.primaryShelf, ...app.secondaryShelves, ...app.audiences, ...app.capabilities],
    trust: "approved-catalog",
    source: {
      kind: "catalog",
      ref: "nexus-cloud:/api/v1/catalog",
      revision: app.reviewStatus === "approved" ? "approved" : "discovered",
    },
  };
}

export function buildKnowledgeResponse(
  catalog: readonly KnowledgeCatalogApp[],
  options: KnowledgeBuildOptions,
): KnowledgeResponse {
  const catalogDocuments = [...catalog]
    .sort((a, b) => a.id.localeCompare(b.id))
    .map(catalogDocument);
  return {
    protocol: "nexus-knowledge/1",
    status: options.status,
    generatedAt: options.generatedAt,
    source: {
      status: options.status,
      cloudUrl: options.cloudUrl,
      catalogEndpoint: `${options.cloudUrl.replace(/\/$/, "")}/api/v1/catalog`,
    },
    documents: [...PLATFORM_DOCUMENTS, ...catalogDocuments],
  };
}

function searchTokens(value: string): string[] {
  return value.toLowerCase().split(/[^a-z0-9/-]+/).filter(Boolean);
}

export function searchKnowledge(
  documents: readonly KnowledgeDocument[],
  query: string,
): KnowledgeDocument[] {
  const tokens = searchTokens(query);
  if (tokens.length === 0) return [...documents];
  return documents.filter((document) => {
    const haystack = [document.title, document.summary, document.content, ...document.tags]
      .join(" ")
      .toLowerCase();
    return tokens.every((token) => haystack.includes(token));
  });
}

export function platformKnowledgeDocuments(): KnowledgeDocument[] {
  return PLATFORM_DOCUMENTS.map((document) => ({ ...document, tags: [...document.tags] }));
}
