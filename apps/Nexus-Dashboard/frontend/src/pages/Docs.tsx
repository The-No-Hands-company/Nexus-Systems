import { useState, useMemo } from "react";
import { Link } from "react-router-dom";
import { APP_GUIDES } from "../shell/appGuides";

type DocSection = {
  id: string;
  category: string;
  title: string;
  description: string;
  content: string[];
  codeBlock?: {
    language: string;
    code: string;
  };
  links?: { label: string; url: string; external?: boolean }[];
};

const BASE_DOC_SECTIONS: DocSection[] = [
  {
    id: "overview",
    category: "Architecture",
    title: "Platform Overview & Sovereignty",
    description: "The core design principles, topology, and sovereign architecture of Nexus Systems.",
    content: [
      "Nexus Systems is an integrated, self-hostable ecosystem designed and operated by The No Hands Company (TNHC). It provides sovereign alternatives to commercial cloud platforms with zero vendor lock-in.",
      "All services run in lean processes managed via a unified gate proxy and Caddy delivery plane. Each application runs either in shell-native mode (sharing the dashboard frame) or external mode (dedicated subdomain under *.tnhc.dev).",
      "Authentication and session state are managed by Nexus Auth using secure, HTTP-only session cookies and Ed25519 identity signatures across all ecosystem subdomains."
    ],
    codeBlock: {
      language: "bash",
      code: `# Verify status of all local services on a Nexus node
./deploy/production/deploy.sh status`
    },
    links: [
      { label: "TNHC Charter", url: "https://tnhc.dev/charter", external: true },
      { label: "System Status & Canaries", url: "https://status.tnhc.dev", external: true }
    ]
  },
  {
    id: "apps-inventory",
    category: "Catalog",
    title: "The 26 Core Platform Applications",
    description: "The official registry of active, released services running across the Nexus ecosystem.",
    content: [
      "The platform provides exactly 26 core sovereign applications grouped into six primary shelves:",
      "1. Work Shelf: Nexus Mail (mail.tnhc.dev :3140), Calendar (calendar.tnhc.dev :3068), Project (project.tnhc.dev :3152), Docs (docs-app.tnhc.dev :3062), Notes (notes.tnhc.dev :3064), Wiki (wiki.tnhc.dev :3066).",
      "2. Create Shelf: Nexus Design (design.tnhc.dev :3074), Graphic (graphic.tnhc.dev :3080), Photos (photos.tnhc.dev :3096), Video (video.tnhc.dev :3113), Draw (draw.tnhc.dev :3070).",
      "3. Developer Shelf: Nexus Deploy (deploy.tnhc.dev :3150), Code (code.tnhc.dev :3072), Terminal (:3110), Modeling (:3094), AI (:3071).",
      "4. Operate Shelf: Nexus Cloud (cloud.tnhc.dev :3060), Edge (:3069), Guardian (:3061), Tunnel (:3063), Router (:3067), Database (:3088), Vault (:3112).",
      "5. Connect Shelf: Nexus Chat (chat.tnhc.dev :3142), Hosting (hosting.tnhc.dev :3084), Dashboard (app.tnhc.dev :3000), Auth (auth.tnhc.dev :4310)."
    ],
    codeBlock: {
      language: "bash",
      code: `# Inspect active registered services
bun run dhts/nexus-v1-radar/scan.ts`
    },
    links: [
      { label: "App Grid", url: "/apps" },
      { label: "System Health Status", url: "https://status.tnhc.dev", external: true }
    ]
  },
  {
    id: "v1-radar",
    category: "Radar",
    title: "v1.0 Production Maturity Radar",
    description: "Comprehensive 6-pillar feature completeness scorecards and production roadmaps.",
    content: [
      "To ensure every application meets high enterprise quality standards, the Nexus v1.0 Production Maturity Radar audits applications across 6 core pillars:",
      "1. Core Engine & Business Logic: CRUD integrity, SQLite/Postgres persistence, data validation.",
      "2. User Interface & Theme Compliance: Design system adoption, Void/Slate tokens, responsiveness, WCAG 2.1 AA keyboard navigation.",
      "3. API & Protocol Completeness: OpenAPI contracts, error handling, rate limiting.",
      "4. Security & Sovereign Auth: Ed25519 signatures, role-based access control, input sanitization.",
      "5. Observability & Quality Gates: Passing check.sh scripts, unit/integration tests, /health endpoints.",
      "6. Offline & Self-Hosting Readiness: Rootless operation, zero external cloud dependencies, Docker/systemd service configs.",
      "The radar is available both as a CLI tool (bun run radar) and an interactive web server at http://127.0.0.1:3200."
    ],
    codeBlock: {
      language: "bash",
      code: `# Run the Maturity Radar CLI
bun run radar

# Launch the interactive Radar Web UI
bun run radar:server
# Open in browser: http://127.0.0.1:3200`
    },
    links: [
      { label: "Radar Web Dashboard", url: "http://127.0.0.1:3200", external: true }
    ]
  },
  {
    id: "auth-did",
    category: "Security",
    title: "Sovereign Auth & Ed25519 DIDs",
    description: "Decentralized cryptographic identity, zero-knowledge proofs, and secure cross-domain SSO.",
    content: [
      "Nexus Auth manages identity without tracking or third-party identity providers. Every account is assigned a decentralized identifier (DID) backed by Ed25519 keypairs.",
      "Sessions use secure HTTP-only cookies partitioned across *.tnhc.dev domains with SameSite=Lax protection. The dashboard shell automatically handles identity refresh, token validation, and multi-device session revocation."
    ],
    links: [
      { label: "Open Auth Portal", url: "https://auth.tnhc.dev", external: true },
      { label: "Account Settings", url: "/account" }
    ]
  },
  {
    id: "infra-ops",
    category: "Operate",
    title: "Infrastructure & Node Operations",
    description: "Caddy reverse proxy, rootless isolation, service watchdog, and unified deployment scripts.",
    content: [
      "All services are managed by the unified production deployment script at deploy/production/deploy.sh.",
      "The ecosystem includes built-in automated health checks, dynamic rate limiters, and systemd watchdog supervision to ensure 99.99% uptime with self-healing process restarts."
    ],
    codeBlock: {
      language: "bash",
      code: `# Start or restart the ecosystem
bash deploy/production/deploy.sh start
bash deploy/production/deploy.sh restart dashboard
bash deploy/production/deploy.sh status`
    }
  },
  {
    id: "nexus-deploy",
    category: "Developer",
    title: "Nexus Deploy — Push-to-Deploy Platform",
    description: "Deploy Git repositories or container images with automatic HTTPS, rootless sandboxing, and zero-downtime rollbacks.",
    content: [
      "Nexus Deploy is a free, open-source alternative to platforms like Railway and Render. It allows developers to connect any Git repository or container image to get a live, secure HTTPS application under *.tnhc.dev or custom domains.",
      "Rootless Sandboxing: All customer builds and containers run under a dedicated unprivileged user namespace with rootless Docker, dropped capabilities (CapEff=0), and strict memory/CPU cgroup limits.",
      "Loopback & LAN Isolation: Sandboxed containers cannot access host loopback services or local network subnets, preventing lateral movement or port scanning.",
      "Zero-Config Railpack: Repositories without Dockerfiles are automatically analyzed by Railpack (Node.js, Python, Rust, Go, Bun) and packaged into minimal OCI containers."
    ],
    codeBlock: {
      language: "bash",
      code: `# Run Deploy preflight doctor checks (13/13 sandbox tests)
cd apps/Nexus-Deploy && npm run doctor

# Web UI and control plane available at:
https://deploy.tnhc.dev`
    },
    links: [
      { label: "Open Nexus Deploy", url: "https://deploy.tnhc.dev", external: true }
    ]
  },
  {
    id: "document-suite",
    category: "Work & Docs",
    title: "Document & Knowledge Suite",
    description: "Sovereign knowledge management, structured wiki articles, and rich-text document editing.",
    content: [
      "Nexus Wiki (wiki.tnhc.dev): High-performance FastAPI and SQLite wiki engine for structured team knowledge bases, category indexes, revisions history, and automatic backlinks.",
      "Nexus Notes (notes.tnhc.dev): Sovereign Markdown note-taking app with live side-by-side preview, custom notebooks, tag filtering, and pin shortcuts.",
      "Nexus Docs (docs-app.tnhc.dev): Modern rich-text document workspace with folders, covers, icons, callout blocks, checkboxes, and inline code formatting."
    ],
    codeBlock: {
      language: "bash",
      code: `# Run quality gates across the Document Suite
cd apps/Nexus-Wiki && ./check.sh
cd apps/Nexus-Notes && ./check.sh
cd apps/Nexus-Docs && ./check.sh`
    },
    links: [
      { label: "Open Wiki", url: "https://wiki.tnhc.dev", external: true },
      { label: "Open Notes", url: "https://notes.tnhc.dev", external: true },
      { label: "Open Docs", url: "https://docs-app.tnhc.dev", external: true }
    ]
  },
  {
    id: "creative-suite",
    category: "Create",
    title: "The Creative Studio Suite",
    description: "Interface design, vector illustration, video streaming, and media asset library.",
    content: [
      "Nexus Design (design.tnhc.dev): Professional UI and component design tool with infinite vector canvas, layers, styling inspectors, and asset export.",
      "Nexus Graphic (graphic.tnhc.dev): Vector graphic illustration studio for icons, logos, and diagram assets.",
      "Nexus Photos (photos.tnhc.dev): Private photo library with album organization, EXIF metadata extraction, and thumbnail processing pipelines.",
      "Nexus Video (video.tnhc.dev): Video streaming, playlists, video player, and sovereign media catalog."
    ],
    codeBlock: {
      language: "bash",
      code: `# Creative suite services ports
Nexus-Design:  http://127.0.0.1:3074 -> https://design.tnhc.dev
Nexus-Graphic: http://127.0.0.1:3080 -> https://graphic.tnhc.dev
Nexus-Photos:  http://127.0.0.1:3096 -> https://photos.tnhc.dev
Nexus-Video:   http://127.0.0.1:3113 -> https://video.tnhc.dev`
    },
    links: [
      { label: "Open Design", url: "https://design.tnhc.dev", external: true },
      { label: "Open Graphic", url: "https://graphic.tnhc.dev", external: true },
      { label: "Open Photos", url: "https://photos.tnhc.dev", external: true },
      { label: "Open Video", url: "https://video.tnhc.dev", external: true }
    ]
  },
  {
    id: "developer-services",
    category: "Developer",
    title: "Developer & Infrastructure Services",
    description: "Git repository viewer, encrypted secrets vault, terminal sessions, and container orchestration.",
    content: [
      "Nexus Code (code.tnhc.dev): Sovereign Git repository viewer, syntax-highlighted source browser, branch navigator, commit history tree, and gists/snippets.",
      "Nexus Vault (vault.tnhc.dev): Encrypted API key and secrets manager with AES-256 derivation, audit logs verification, and role-based token provisioning.",
      "Nexus Terminal (Port 3110): Browser terminal with multi-tab pty sessions and authenticated operator access."
    ],
    codeBlock: {
      language: "bash",
      code: `# Quality checks for Developer Suite
cd apps/Nexus-Code && ./check.sh
cd apps/Nexus-Vault && ./check.sh
cd apps/Nexus-Terminal && ./check.sh`
    },
    links: [
      { label: "Open Code", url: "https://code.tnhc.dev", external: true },
      { label: "Open Vault", url: "https://vault.tnhc.dev", external: true }
    ]
  },
  {
    id: "apps",
    category: "Applications",
    title: "Ecosystem Core Applications",
    description: "Guide to the primary communication and productivity tools running on Nexus.",
    content: [
      "Nexus Chat (chat.tnhc.dev): High-performance messaging, channels, and voice chat powered by Axum and WebSockets.",
      "Nexus Mail (mail.tnhc.dev): Sovereign email web client with end-to-end HTML sanitization and background SMTP/IMAP integration via mailapi (:3140).",
      "Nexus Calendar (calendar.tnhc.dev): Shared calendars, event scheduling, RSVP invites, and month/week agenda views.",
      "Nexus Project (project.tnhc.dev): Work breakdown structure (WBS), Kanban boards, milestone tracking, and critical path scheduling.",
      "Nexus Draw (draw.tnhc.dev): Collaborative real-time whiteboard, architecture sketching, and wireframing.",
      "Nexus Hosting (hosting.tnhc.dev): Decentralized static site hosting network powered by MinIO S3 and Caddy."
    ],
    links: [
      { label: "Browse All Apps", url: "/apps" }
    ]
  },
  {
    id: "developer-apis",
    category: "Developer",
    title: "Developer APIs & SDKs",
    description: "REST facades, client-tag conventions, session authentication, and SDK packages.",
    content: [
      "Platform Facade: All dynamic platform requests travel through /ipa/v1/* routes on the dashboard host (app.tnhc.dev) or api.tnhc.dev. Requests carry the nexus_session cookie.",
      "Rate Limiting: Dynamic API endpoints enforce strict token buckets (200 token burst, 50 tokens/sec refill). Static assets and /health endpoints are excluded to guarantee smooth client navigation.",
      "Shared Packages: Development kits available in packages/ include phantom-sdk (cryptographic and transport primitives) and nexus-sdk (federation and client-side helpers)."
    ],
    codeBlock: {
      language: "typescript",
      code: `// Fetch current identity from the platform facade
const res = await fetch("/ipa/v1/me", {
  credentials: "include",
  headers: { "accept": "application/json" }
});
const { user } = await res.json();`
    }
  },
  {
    id: "node-operators",
    category: "Operations",
    title: "Node Operators & Federation",
    description: "Running a federated Nexus node on personal hardware, VPS, or cloud instances.",
    content: [
      "Anyone can operate a Nexus node to federate applications, storage, and identity while retaining total control over their data.",
      "Minimum Hardware: Raspberry Pi 4 or equivalent (2 cores, 2GB RAM, 10GB storage). Linux with modern systemd and rootless container support.",
      "Watchdog & Health Reconciler: Production deployments include automated watchdog scripts (nexus-watchdog.sh) that monitor port health and reconcile PIDs without heavy container restarts."
    ],
    links: [
      { label: "Node Operators Guide", url: "/node-operators" }
    ]
  },
  {
    id: "handbook",
    category: "Organization",
    title: "Company Handbook & Policies",
    description: "The No Hands Company governance, privacy policy, and brand standards.",
    content: [
      "The No Hands Company operates under an open charter prioritizing digital sovereignty, user privacy, transparent operations, and community ownership.",
      "Warrant & Privacy Canaries: Status.tnhc.dev publishes automated daily cryptographic audits confirming zero government data subpoenas and zero database access breaches.",
      "Security Disclosures: Potential vulnerabilities should be reported through the dashboard Report Issue form or directly to security@tnhc.dev."
    ],
    links: [
      { label: "Read Charter", url: "https://tnhc.dev/charter", external: true },
      { label: "Report an Issue", url: "/report" }
    ]
  }
];

const APP_DOC_SECTIONS: DocSection[] = Object.values(APP_GUIDES).map((guide) => ({
  id: guide.docSectionId,
  category: guide.category,
  title: `${guide.name} — Architecture & User Guide`,
  description: guide.tagline,
  content: [
    `What it is: ${guide.whatIsIt}`,
    `When to use: ${guide.whenToUse}`,
    `Core Capabilities: ${guide.topCapabilities.map((c) => `${c.title} (${c.desc})`).join(" • ")}.`,
    `Quick Start Walkthrough: Step 1: ${guide.tutorialSteps[0]?.title} — ${guide.tutorialSteps[0]?.description}. Step 2: ${guide.tutorialSteps[1]?.title} — ${guide.tutorialSteps[1]?.description}. Step 3: ${guide.tutorialSteps[2]?.title} — ${guide.tutorialSteps[2]?.description}.`,
    ...(guide.proTips.length > 0 ? [`Power User Tips: ${guide.proTips.join(" ")}`] : []),
  ],
  links: [
    { label: `Open ${guide.name} Launchpad & Tutorial`, url: `/${guide.id.replace(/^nexus-/, "")}`, external: false },
    { label: "Status & Uptime", url: "https://status.tnhc.dev", external: true },
  ],
}));

const DOC_SECTIONS: DocSection[] = [...BASE_DOC_SECTIONS, ...APP_DOC_SECTIONS];

export default function Docs() {
  const [activeCategory, setActiveCategory] = useState<string>("All");
  const [searchQuery, setSearchQuery] = useState<string>("");

  const categories = useMemo(() => {
    const cats = Array.from(new Set(DOC_SECTIONS.map((s) => s.category)));
    return ["All", ...cats];
  }, []);

  const filteredSections = useMemo(() => {
    return DOC_SECTIONS.filter((section) => {
      const matchesCategory = activeCategory === "All" || section.category === activeCategory;
      const query = searchQuery.toLowerCase().trim();
      const matchesSearch =
        !query ||
        section.title.toLowerCase().includes(query) ||
        section.description.toLowerCase().includes(query) ||
        section.content.some((c) => c.toLowerCase().includes(query));
      return matchesCategory && matchesSearch;
    });
  }, [activeCategory, searchQuery]);

  return (
    <div className="mx-auto max-w-5xl space-y-8 px-4 py-6 sm:p-8">
      {/* Header */}
      <div>
        <div className="flex items-center gap-2 text-xs font-semibold uppercase tracking-wider text-accent">
          <span>Documentation Hub</span>
          <span>•</span>
          <span>Nexus Systems & TNHC</span>
        </div>
        <h1 className="mt-2 text-3xl font-bold tracking-tight text-zinc-100 sm:text-4xl">
          Platform Documentation
        </h1>
        <p className="mt-2 max-w-2xl text-base text-zinc-400">
          Everything you need to build, deploy, operate, and integrate with Nexus Cloud, sovereign apps, and node infrastructure.
        </p>
      </div>

      {/* Search & Category Filter Controls */}
      <div className="flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
        <div className="relative flex-1 max-w-md">
          <input
            type="search"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search documentation, guides, APIs…"
            className="w-full rounded-lg border border-zinc-700 bg-zinc-900/80 px-4 py-2 pl-10 text-sm text-zinc-100 placeholder-zinc-500 focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent"
          />
          <svg
            className="pointer-events-none absolute left-3 top-2.5 h-4 w-4 text-zinc-500"
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
            aria-hidden="true"
          >
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z" />
          </svg>
        </div>

        <div className="flex flex-wrap items-center gap-1.5">
          {categories.map((cat) => (
            <button
              key={cat}
              type="button"
              onClick={() => setActiveCategory(cat)}
              className={`rounded-md px-3 py-1.5 text-xs font-medium transition-colors ${
                activeCategory === cat
                  ? "bg-accent text-accent-foreground font-semibold"
                  : "bg-zinc-800/60 text-zinc-400 hover:bg-zinc-800 hover:text-zinc-200"
              }`}
            >
              {cat}
            </button>
          ))}
        </div>
      </div>

      {/* Docs Grid */}
      <div className="space-y-6">
        {filteredSections.length === 0 ? (
          <div className="rounded-lg border border-zinc-800 bg-zinc-900/50 p-8 text-center text-zinc-400">
            No documentation matched your query "{searchQuery}".
          </div>
        ) : (
          filteredSections.map((section) => (
            <article
              key={section.id}
              id={section.id}
              className="rounded-xl border border-zinc-800 bg-zinc-900/70 p-6 shadow-sm transition-colors hover:border-zinc-700"
            >
              <div className="flex items-center justify-between gap-3">
                <span className="rounded-full bg-zinc-800 px-2.5 py-0.5 text-xs font-medium text-zinc-400">
                  {section.category}
                </span>
              </div>

              <h2 className="mt-3 text-xl font-semibold text-zinc-100">
                {section.title}
              </h2>
              <p className="mt-1 text-sm text-zinc-400 font-normal">
                {section.description}
              </p>

              <div className="mt-4 space-y-2 text-sm text-zinc-300">
                {section.content.map((paragraph, idx) => (
                  <p key={idx} className="leading-relaxed">
                    {paragraph}
                  </p>
                ))}
              </div>

              {section.codeBlock && (
                <div className="mt-4 overflow-hidden rounded-lg border border-zinc-800 bg-black/50">
                  <div className="border-b border-zinc-800/80 px-4 py-1.5 text-xs font-mono text-zinc-500">
                    {section.codeBlock.language}
                  </div>
                  <pre className="overflow-x-auto p-4 text-xs font-mono leading-relaxed text-zinc-300">
                    <code>{section.codeBlock.code}</code>
                  </pre>
                </div>
              )}

              {section.links && section.links.length > 0 && (
                <div className="mt-5 flex flex-wrap items-center gap-3 border-t border-zinc-800/60 pt-4">
                  {section.links.map((link) =>
                    link.external ? (
                      <a
                        key={link.url}
                        href={link.url}
                        target="_blank"
                        rel="noreferrer"
                        className="inline-flex items-center gap-1 text-xs font-medium text-accent hover:underline"
                      >
                        {link.label} ↗
                      </a>
                    ) : (
                      <Link
                        key={link.url}
                        to={link.url}
                        className="inline-flex items-center gap-1 text-xs font-medium text-accent hover:underline"
                      >
                        {link.label} →
                      </Link>
                    )
                  )}
                </div>
              )}
            </article>
          ))
        )}
      </div>
    </div>
  );
}
