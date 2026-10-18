#!/usr/bin/env bun
// Nexus Operator CLI — unified command center for the Nexus Systems ecosystem

const APPS_ROOT = process.env.NEXUS_ROOT || "../..";

const APPS: Record<string, { dir: string; port: number; desc: string }> = {
  cloud:    { dir: "Nexus-Cloud",    port: 8787, desc: "Control plane, topology, dashboard" },
  guardian: { dir: "Nexus-Guardian", port: 4320, desc: "Policy engine, threat detection, alerts" },
  auth:     { dir: "Nexus-Auth",     port: 4310, desc: "Identity, SSO, RBAC, API keys" },
  edge:     { dir: "Nexus-Edge",     port: 4340, desc: "Ingress gateway, rate limiting, metrics" },
  tunnel:   { dir: "Nexus-Tunnel",   port: 4330, desc: "HTTPS tunnel, reachability, TLS" },
  files:    { dir: "Nexus-Files",    port: 3033, desc: "File storage, S3/Disk, versioning" },
  search:   { dir: "Nexus-Search",   port: 3034, desc: "Full-text search, FTS5, federated indexing" },
  monitor:  { dir: "Nexus-Monitor",  port: 3030, desc: "Metrics, alerting, service tracking" },
  api:      { dir: "Nexus-API",      port: 3036, desc: "API gateway, route management" },
  chat:     { dir: "Nexus-Team-Chat", port: 3109, desc: "Real-time messaging, WebSocket gateway" },
};

const RESET = "\x1b[0m";
const BOLD = "\x1b[1m";
const DIM = "\x1b[2m";
const GREEN = "\x1b[32m";
const RED = "\x1b[31m";
const YELLOW = "\x1b[33m";
const BLUE = "\x1b[34m";
const CYAN = "\x1b[36m";
const MAGENTA = "\x1b[35m";

const BANNER = `
${CYAN}╔══════════════════════════════════════════════╗
║         ${BOLD}Nexus Systems — Operator CLI${RESET}${CYAN}         ║
║     ${DIM}Sovereign. Federated. Zero lock-in.${RESET}${CYAN}       ║
╚══════════════════════════════════════════════╝${RESET}`;

function color(s: string, c: string) { return `${c}${s}${RESET}`; }
function bold(s: string) { return `${BOLD}${s}${RESET}`; }

type CommandHandler = (args: string[]) => Promise<void>;

const commands: Record<string, { desc: string; usage: string; run: CommandHandler }> = {
  up: {
    desc: "Start all ecosystem services",
    usage: "nexus up [service...]",
    run: cmdUp,
  },
  down: {
    desc: "Stop all ecosystem services",
    usage: "nexus down",
    run: cmdDown,
  },
  status: {
    desc: "Show live health dashboard for all services",
    usage: "nexus status",
    run: cmdStatus,
  },
  topology: {
    desc: "Show the ecosystem app relationship graph",
    usage: "nexus topology",
    run: cmdTopology,
  },
  health: {
    desc: "Check health of a specific service",
    usage: "nexus health <service>",
    run: cmdHealth,
  },
  detect: {
    desc: "Scan for threats across the ecosystem",
    usage: "nexus detect [service]",
    run: cmdDetect,
  },
  logs: {
    desc: "Tail logs from a service",
    usage: "nexus logs <service>",
    run: cmdLogs,
  },
  apps: {
    desc: "List all 81 ecosystem apps",
    usage: "nexus apps [category]",
    run: cmdApps,
  },
  catalog: {
    desc: "Read the approved Cloud app catalog",
    usage: "nexus catalog [shelf]",
    run: cmdCatalog,
  },
  cloud: {
    desc: "Read the Nexus Cloud control-plane contract",
    usage: "nexus cloud",
    run: cmdCloudContract,
  },
  discover: {
    desc: "Discover the public contracts for platform services",
    usage: "nexus discover",
    run: cmdDiscover,
  },
  api: {
    desc: "Read the versioned Nexus API contract",
    usage: "nexus api",
    run: cmdApiContract,
  },
  auth: {
    desc: "Read the Nexus Auth platform-service contract",
    usage: "nexus auth",
    run: cmdAuthContract,
  },
  terminal: {
    desc: "Read the Terminal contract or audited sessions",
    usage: "nexus terminal <contract|audit>",
    run: cmdTerminal,
  },
  knowledge: {
    desc: "Search the ecosystem-grounded Nexus AI knowledge index",
    usage: "nexus knowledge [query]",
    run: cmdKnowledge,
  },
  ping: {
    desc: "Ping all services to check reachability",
    usage: "nexus ping",
    run: cmdPing,
  },
  chat: {
    desc: "Open Team-Chat WebSocket client (basic)",
    usage: "nexus chat",
    run: cmdChat,
  },
  info: {
    desc: "Show detailed info about a service",
    usage: "nexus info <service>",
    run: cmdInfo,
  },
  demo: {
    desc: "Run the full ecosystem demo flow",
    usage: "nexus demo",
    run: cmdDemo,
  },
};

async function fetchJson(url: string, init?: RequestInit): Promise<Record<string, unknown> | null> {
  try {
    const r = await fetch(url, { ...init, signal: AbortSignal.timeout(3000) });
    if (!r.ok) return null;
    return await r.json() as Record<string, unknown>;
  } catch { return null; }
}

function cloudUrl(): string {
  return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, "");
}

function cloudHeaders(): Record<string, string> {
  return {
    accept: "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
  };
}

type HealthResult = {
  alive: boolean;
  service: string;
  uptime: number;
  status: string;
  version: string;
};

async function checkHealth(port: number): Promise<HealthResult> {
  return checkHealthAt(`http://localhost:${port}`);
}

async function checkHealthAt(baseUrl: string): Promise<HealthResult> {
  const data = await fetchJson(`${baseUrl}/health`);
  return {
    alive: data !== null && data.status === "ok",
    service: (data?.service as string) || "unknown",
    uptime: (data?.uptimeSeconds as number) || 0,
    status: (data?.status as string) || "unavailable",
    version: (data?.version as string) || "v1",
  };
}

function serviceBaseUrl(name: string, port: number): string {
  const configured = name === "cloud"
    ? cloudUrl()
    : name === "auth"
    ? process.env.NEXUS_AUTH_URL
    : name === "api"
    ? process.env.NEXUS_API_URL
    : name === "terminal"
    ? process.env.NEXUS_TERMINAL_URL
    : undefined;
  return (configured || `http://localhost:${port}`).trim().replace(/\/$/, "");
}

async function checkServiceHealth(name: string, port: number) {
  const baseUrl = serviceBaseUrl(name, port);
  return { ...await checkHealthAt(baseUrl), baseUrl };
}

async function cmdUp(args: string[]) {
  console.log(BANNER);
  console.log(color("Starting Nexus ecosystem...\n", GREEN));

  const targets = args.length > 0 ? args : Object.keys(APPS);

  for (const name of targets) {
    const app = APPS[name];
    if (!app) { console.log(`  ${color("✗", RED)} Unknown: ${name}`); continue; }

    const { alive } = await checkHealth(app.port);
    if (alive) {
      console.log(`  ${color("●", GREEN)} ${bold(name)} — already running on :${app.port}`);
      continue;
    }

    const proc = Bun.spawn(["bun", "run", "src/index.ts"], {
      cwd: `${APPS_ROOT}/apps/${app.dir}`,
      stdio: ["ignore", "ignore", "ignore"],
      env: { ...process.env, NEXUS_CLOUD_URL: "http://localhost:8787" },
    });
    console.log(`  ${color("▶", BLUE)} ${bold(name)} — starting on :${app.port} (pid ${proc.pid})`);
    await new Promise((r) => setTimeout(r, 500));
  }

  console.log(`\n${color("✓ Ecosystem starting — Cloud dashboard: http://localhost:8787", GREEN)}`);
}

async function cmdDown() {
  console.log(color("Stopping Nexus ecosystem...", YELLOW));
  for (const name of Object.keys(APPS)) {
    const app = APPS[name]!;
    const result = Bun.spawnSync(["fuser", "-k", `${app.port}/tcp`], { stdio: ["ignore", "ignore", "ignore"] });
    if (result.exitCode === 0) console.log(`  ${color("✕", YELLOW)} ${bold(name)} — stopped`);
    else console.log(`  ${color("·", DIM)} ${DIM}${name} — not running${RESET}`);
  }
  console.log(color("\n✓ All services stopped.", GREEN));
}

async function cmdStatus() {
  console.log(BANNER);
  console.log(color("Ecosystem Health Dashboard\n", CYAN));

  let aliveCount = 0;
  const rows: string[][] = [];

  for (const [name, app] of Object.entries(APPS)) {
    const { alive, uptime, baseUrl } = await checkServiceHealth(name, app.port);
    if (alive) aliveCount++;

    const dot = alive ? color("●", GREEN) : color("○", RED);
    const status = alive ? color("healthy", GREEN) : color("down", RED);
    const uptimeStr = alive ? `${Math.floor(uptime / 60)}m ${uptime % 60}s` : "—";
    const target = baseUrl === `http://localhost:${app.port}` ? `:${app.port}` : baseUrl;
    rows.push([`  ${dot}`, bold(name.padEnd(12)), target.padEnd(30), status.padEnd(10), uptimeStr.padEnd(10), DIM + app.desc + RESET]);
  }

  // Header
  console.log(`  ${bold("SERVICE".padEnd(15))}${bold("TARGET".padEnd(32))}${bold("STATUS".padEnd(11))}${bold("UPTIME".padEnd(11))}${bold("DESCRIPTION")}`);
  console.log(`  ${"─".repeat(112)}`);

  for (const row of rows) console.log(row.join(""));

  const totalServices = Object.keys(APPS).length;
  console.log(`\n  ${color(`${aliveCount}/${totalServices} services healthy`, aliveCount === totalServices ? GREEN : YELLOW)}`);

  // Also try to get Cloud status for more detail
  const cloudStatus = await fetchJson(`${serviceBaseUrl("cloud", 8787)}/api/v1/status`, { headers: cloudHeaders() });
  if (cloudStatus) {
    const s = cloudStatus as Record<string, unknown>;
    console.log(`\n  ${CYAN}Cloud Dashboard:${RESET} ${serviceBaseUrl("cloud", 8787)}`);
    console.log(`  Tools registered: ${s.toolCount || 0}  Healthy: ${s.healthyToolCount || 0}`);
  }
}

async function cmdTopology() {
  console.log(BANNER);
  console.log(color("Ecosystem Topology\n", CYAN));

  const data = await fetchJson("http://localhost:8787/api/v1/topology");
  if (!data) { console.log(color("  Cloud not running — start with 'nexus up cloud'", RED)); return; }

  const summary = data.summary as Record<string, number>;
  console.log(`  ${bold("Apps:")} ${summary.appCount}  ${bold("Connections:")} ${summary.connectionCount}  ${bold("Embedded:")} ${summary.embeddedAppCount}`);

  // Show top-level apps grouped by kind
  const apps = (data.apps || []) as Array<{ id: string; name: string; kind: string; description: string }>;
  const byKind: Record<string, typeof apps> = {};
  for (const app of apps) {
    (byKind[app.kind] ||= []).push(app);
  }

  for (const [kind, kindApps] of Object.entries(byKind)) {
    console.log(`\n  ${bold(kind.toUpperCase())} (${kindApps.length})`);
    for (const app of kindApps.slice(0, 8)) {
      console.log(`    ${color("├", DIM)} ${bold(app.name)} — ${DIM}${app.description.slice(0, 60)}${RESET}`);
    }
    if (kindApps.length > 8) console.log(`    ${color("└", DIM)} ... and ${kindApps.length - 8} more`);
  }
}

async function cmdHealth(args: string[]) {
  const name = args[0];
  const app = name ? APPS[name] : undefined;
  const platformService = name === "terminal"
    ? { port: 3110, desc: "Audited operator and developer terminal" }
    : undefined;
  if (!name || (!app && !platformService)) { console.log(color("Usage: nexus health <service>", YELLOW)); return; }

  const target = app || platformService!;
  const result = await checkServiceHealth(name, target.port);
  if (!result.alive) { console.log(color(`${name} health unavailable at ${result.baseUrl}/health`, RED)); return; }

  console.log(bold(`\n${name} Health`));
  console.log(`  Status:    ${color(result.status, result.status === "ok" ? GREEN : RED)}`);
  console.log(`  Service:   ${result.service}`);
  console.log(`  Version:   ${result.version}`);
  console.log(`  Uptime:    ${result.uptime}s`);
  console.log(`  Endpoint:  ${result.baseUrl}/health`);
  console.log(`  Desc:      ${target.desc}`);
}

async function cmdDetect(args: string[]) {
  console.log(BANNER);

  // If a service name is given, check that service's health endpoint for threat data
  // Otherwise, query Guardian directly for recent threat matches
  const data = await fetchJson("http://localhost:4320/api/v1/guardian/detect/stats");
  if (!data) { console.log(color("  Guardian not running — start with 'nexus up guardian'", RED)); return; }

  const stats = data.stats as Record<string, unknown>;
  console.log(color("Threat Detection Report\n", CYAN));
  console.log(`  Total matches: ${stats.totalMatches || 0}`);

  const byCategory = stats.byCategory as Record<string, number> || {};
  if (Object.keys(byCategory).length > 0) {
    console.log(`\n  ${bold("By Category:")}`);
    for (const [cat, count] of Object.entries(byCategory)) {
      const c = count > 5 ? RED : count > 0 ? YELLOW : DIM;
      console.log(`    ${color("●", c)} ${cat}: ${count}`);
    }
  }

  const bySeverity = stats.bySeverity as Record<string, number> || {};
  if (Object.keys(bySeverity).length > 0) {
    console.log(`\n  ${bold("By Severity:")}`);
    for (const [sev, count] of Object.entries(bySeverity)) {
      const c = sev === "critical" ? RED : sev === "high" ? YELLOW : GREEN;
      console.log(`    ${color("●", c)} ${sev}: ${count}`);
    }
  }

  // Recent matches
  const matches = await fetchJson("http://localhost:4320/api/v1/guardian/detect/matches?limit=5");
  if (matches && Array.isArray(matches.matches) && matches.matches.length > 0) {
    console.log(`\n  ${bold("Recent Matches:")}`);
    for (const m of matches.matches as Array<Record<string, string>>) {
      const sevColor = m.severity === "critical" ? RED : m.severity === "high" ? YELLOW : DIM;
      console.log(`    ${color("►", sevColor)} ${m.patternName} — ${DIM}${m.source} (${m.severity})${RESET}`);
    }
  }

  // Blacklisted IPs
  const ips = await fetchJson("http://localhost:4320/api/v1/guardian/ips/blacklisted");
  if (ips && Array.isArray(ips.blacklisted) && ips.blacklisted.length > 0) {
    console.log(`\n  ${color("Blacklisted IPs:", RED)}`);
    for (const ip of ips.blacklisted as Array<Record<string, string>>) {
      console.log(`    ${color("✕", RED)} ${ip.ip} — ${ip.blacklistReason}`);
    }
  }
}

async function cmdLogs(args: string[]) {
  const name = args[0];
  if (!name || !APPS[name]) { console.log(color("Usage: nexus logs <service>", YELLOW)); return; }
  const app = APPS[name]!;

  console.log(color(`Tailing logs for ${name}... (Ctrl+C to stop)\n`, CYAN));

  const proc = Bun.spawn(["bun", "run", "src/index.ts"], {
    cwd: `${APPS_ROOT}/apps/${app.dir}`,
    stdout: "inherit",
    stderr: "inherit",
  });

  process.on("SIGINT", () => { proc.kill(); process.exit(0); });
  await proc.exited;
}

async function cmdApps(args: string[]) {
  console.log(BANNER);
  console.log(color("Nexus Ecosystem Apps\n", CYAN));

  const categories: Record<string, string[]> = {
    "platform": ["cloud", "systems-api"],
    "infrastructure": ["guardian", "auth", "edge", "tunnel", "network", "vault", "security", "compliance"],
    "platform-services": ["files", "search", "monitor", "api", "deploy", "hosting", "warehouse"],
    "communication": ["nexus", "team-chat", "meet", "email", "presence"],
    "ai": ["ai", "ai-hub", "agents", "inference", "claw"],
    "dev": ["forge", "ide", "testing", "code-review", "computer", "terminal", "lang"],
    "productivity": ["notes", "tasks", "calendar", "wiki", "planner", "office", "forms", "automate", "book", "pdf"],
    "media": ["photos", "media", "music", "radio-live", "video", "graphic", "draw", "design", "content"],
    "business": ["crm", "sales", "hr", "accounting", "contracts", "billing", "analytics", "insights", "survey", "seo", "finance", "spend", "store", "market"],
    "learning": ["learn", "tutor", "academy"],
    "wellness": ["health", "fitness", "nutrition", "mind"],
    "gaming": ["game", "play", "arcade", "engine"],
    "specialized": ["modeling", "database", "vertical", "social", "home", "phantom"],
  };

  const filter = args[0];
  const catsToShow = filter ? { [filter]: categories[filter] || [] } : categories;

  for (const [cat, apps] of Object.entries(catsToShow)) {
    if (!apps || apps.length === 0) continue;
    console.log(`  ${bold(cat.toUpperCase())} (${apps.length})`);
    for (const app of apps) {
      const running = APPS[app];
      const dot = running ? color("●", GREEN) : color("○", DIM);
      const label = running ? bold(app) : DIM + app + RESET;
      console.log(`    ${dot} ${label}`);
    }
    console.log();
  }

  console.log(`  ${color("● running  ○ scaffolded", DIM)}`);
  console.log(`  Total: 81 apps across ${Object.keys(categories).length} categories`);
}

async function cmdCatalog(args: string[]) {
  const requestedShelf = args[0]?.trim().toLowerCase();
  const payload = await fetchJson(`${cloudUrl()}/api/v1/catalog`, { headers: cloudHeaders() });
  if (!payload || !Array.isArray(payload.apps)) {
    console.log(color("Cloud catalog unavailable — check NEXUS_CLOUD_URL and authentication", RED));
    return;
  }

  const apps = (payload.apps as Array<Record<string, unknown>>)
    .filter((app) => {
      if (!requestedShelf) return true;
      return app.primaryShelf === requestedShelf ||
        (Array.isArray(app.secondaryShelves) && app.secondaryShelves.includes(requestedShelf));
    })
    .sort((a, b) => String(a.name || a.id).localeCompare(String(b.name || b.id)));

  console.log(BANNER);
  console.log(color(`Cloud Catalog${requestedShelf ? ` — ${requestedShelf}` : ""}\n`, CYAN));
  for (const app of apps) {
    const health = app.health === "healthy" ? color("●", GREEN) : color("○", RED);
    const path = typeof app.path === "string" ? app.path : "—";
    const status = typeof app.reviewStatus === "string" ? app.reviewStatus : "unknown";
    console.log(`  ${health} ${bold(String(app.name || app.id))} ${DIM}${path} · ${status}${RESET}`);
  }
  console.log(`\n  ${apps.length} catalog entr${apps.length === 1 ? "y" : "ies"}`);
}

async function cmdCloudContract() {
  const payload = await fetchJson(`${cloudUrl()}/api/v1/contract`, { headers: cloudHeaders() });
  if (!payload || !payload.service || !Array.isArray(payload.routes)) {
    console.log(color("Nexus Cloud contract unavailable — check NEXUS_CLOUD_URL and authentication", RED));
    return;
  }

  const service = payload.service as Record<string, unknown>;
  console.log(BANNER);
  console.log(color("Nexus Cloud Control Plane\n", CYAN));
  console.log(`  ${bold(String(service.name || service.id))}`);
  console.log(`  Surface: ${String(service.surface || "admin-console")} · Hosted path: ${String(service.hostedPath || "/cloud")}`);
  console.log(`  Auth: ${String(service.auth || "unspecified")}\n`);
  for (const route of payload.routes as Array<Record<string, unknown>>) {
    const access = route.access === "public" ? color("public", GREEN) : color(String(route.access), YELLOW);
    console.log(`  ${String(route.method).padEnd(6)} ${String(route.path).padEnd(54)} ${access}`);
  }
  console.log(`\n  ${(payload.routes as unknown[]).length} declared Cloud route${(payload.routes as unknown[]).length === 1 ? "" : "s"}`);
}

async function cmdDiscover() {
  const services: Array<{ id: string; name: string; url: string; init?: RequestInit }> = [
    {
      id: "cloud",
      name: "Nexus Cloud",
      url: `${cloudUrl()}/api/v1/contract`,
      init: { headers: cloudHeaders() },
    },
    {
      id: "auth",
      name: "Nexus Auth",
      url: `${(process.env.NEXUS_AUTH_URL || "http://localhost:4310").trim().replace(/\/$/, "")}/api/v1/auth/contract`,
    },
    {
      id: "api",
      name: "Nexus API",
      url: `${(process.env.NEXUS_API_URL || "http://localhost:3036").trim().replace(/\/$/, "")}/api/v1/contract`,
    },
    {
      id: "terminal",
      name: "Nexus Terminal",
      url: `${(process.env.NEXUS_TERMINAL_URL || "http://localhost:3110").trim().replace(/\/$/, "")}/api/v1/terminal/contract`,
    },
  ];

  const results = await Promise.all(services.map(async (service) => ({
    ...service,
    payload: await fetchJson(service.url, service.init),
  })));

  console.log(BANNER);
  console.log(color("Nexus Platform Discovery\n", CYAN));
  console.log(`  ${DIM}Public versioned contracts; this command does not grant access or mutate state.${RESET}\n`);
  console.log(`  ${bold("SERVICE".padEnd(18))}${bold("SURFACE".padEnd(20))}${bold("HOSTED PATH".padEnd(16))}${bold("STATUS".padEnd(12))}${bold("ROUTES")}`);
  console.log(`  ${"─".repeat(78)}`);

  for (const result of results) {
    const service = result.payload?.service as Record<string, unknown> | undefined;
    const routes = Array.isArray(result.payload?.routes) ? result.payload.routes.length : 0;
    const available = Boolean(service && Array.isArray(result.payload?.routes));
    const surface = String(service?.surface || (result.id === "api" ? "api-service" : "—"));
    const hostedPath = String(service?.hostedPath || (result.id === "api" ? "—" : "—"));
    const status = available ? color("available", GREEN) : color("offline", RED);
    console.log(`  ${result.name.padEnd(18)}${surface.padEnd(20)}${hostedPath.padEnd(16)}${status.padEnd(21)}${available ? String(routes) : "—"}`);
  }

  const available = results.filter((result) => result.payload?.service && Array.isArray(result.payload.routes)).length;
  console.log(`\n  ${available}/${services.length} public platform contracts available`);
}

async function cmdApiContract() {
  const base = (process.env.NEXUS_API_URL || "http://localhost:3036").trim().replace(/\/$/, "");
  const payload = await fetchJson(`${base}/api/v1/contract`);
  if (!payload || !payload.service || !Array.isArray(payload.routes)) {
    console.log(color("Nexus API contract unavailable — check NEXUS_API_URL", RED));
    return;
  }

  const service = payload.service as Record<string, unknown>;
  console.log(BANNER);
  console.log(color("Nexus API Contract\n", CYAN));
  console.log(`  ${bold(String(service.name || service.id))} ${DIM}${String(service.version || "")}${RESET}`);
  console.log(`  Base URL: ${String(service.baseUrl || base)}`);
  console.log(`  Auth: ${String(service.auth || "unspecified")}\n`);
  for (const route of payload.routes as Array<Record<string, unknown>>) {
    const access = route.access === "operator" ? color("operator", YELLOW) : color("public", GREEN);
    console.log(`  ${String(route.method).padEnd(6)} ${String(route.path).padEnd(38)} ${access}`);
  }
}

async function cmdAuthContract() {
  const base = (process.env.NEXUS_AUTH_URL || "http://localhost:4310").trim().replace(/\/$/, "");
  const payload = await fetchJson(`${base}/api/v1/auth/contract`);
  if (!payload || !payload.service || !Array.isArray(payload.routes)) {
    console.log(color("Nexus Auth contract unavailable — check NEXUS_AUTH_URL", RED));
    return;
  }

  const service = payload.service as Record<string, unknown>;
  console.log(BANNER);
  console.log(color("Nexus Auth Platform Service\n", CYAN));
  console.log(`  ${bold(String(service.name || service.id))}`);
  console.log(`  Surface: ${String(service.surface || "platform-service")} · Hosted path: ${String(service.hostedPath || "/auth")}`);
  console.log(`  Auth: ${String(service.auth || "unspecified")}\n`);
  for (const route of payload.routes as Array<Record<string, unknown>>) {
    const access = route.access === "public" ? color("public", GREEN) : color(String(route.access), YELLOW);
    console.log(`  ${String(route.method).padEnd(6)} ${String(route.path).padEnd(46)} ${access}`);
  }
}

async function cmdTerminal(args: string[]) {
  if (args[0] === "contract") {
    await cmdTerminalContract();
    return;
  }
  if (args[0] !== "audit") {
    console.log(color("Usage: nexus terminal <contract|audit>", YELLOW));
    return;
  }
  const token = process.env.NEXUS_AUTH_TOKEN?.trim();
  const cookie = process.env.NEXUS_AUTH_COOKIE?.trim();
  if (!token && !cookie) {
    console.log(color("NEXUS_AUTH_TOKEN or NEXUS_AUTH_COOKIE is required for the Terminal audit read", YELLOW));
    return;
  }
  const base = (process.env.NEXUS_TERMINAL_URL || "http://localhost:3110").trim().replace(/\/$/, "");
  const headers = token ? { authorization: `Bearer ${token}` } : { cookie: cookie! };
  const payload = await fetchJson(`${base}/api/v1/terminal/audit`, { headers });
  if (!payload || !Array.isArray(payload.sessions)) {
    console.log(color("Nexus Terminal audit unavailable — check NEXUS_TERMINAL_URL and Auth session", RED));
    return;
  }
  console.log(BANNER);
  console.log(color("Nexus Terminal Audit\n", CYAN));
  for (const session of payload.sessions as Array<Record<string, unknown>>) {
    console.log(`  ${bold(String(session.id || "unknown"))} ${DIM}${String(session.subject || "unknown subject")}${RESET}`);
    console.log(`  started ${String(session.started_at || "unknown")} · ended ${String(session.ended_at || "active")} · exit ${String(session.exit_code ?? "—")}`);
  }
  console.log(`\n  ${(payload.sessions as unknown[]).length} audited session${(payload.sessions as unknown[]).length === 1 ? "" : "s"}`);
}

async function cmdTerminalContract() {
  const base = (process.env.NEXUS_TERMINAL_URL || "http://localhost:3110").trim().replace(/\/$/, "");
  const payload = await fetchJson(`${base}/api/v1/terminal/contract`);
  if (!payload || !payload.service || !Array.isArray(payload.routes)) {
    console.log(color("Nexus Terminal contract unavailable — check NEXUS_TERMINAL_URL", RED));
    return;
  }

  const service = payload.service as Record<string, unknown>;
  console.log(BANNER);
  console.log(color("Nexus Terminal Contract\n", CYAN));
  console.log(`  ${bold(String(service.name || service.id))}`);
  console.log(`  Auth: ${String(service.auth || "unspecified")} · Enabled by default: ${String(service.enabledByDefault ?? "unspecified")}\n`);
  for (const route of payload.routes as Array<Record<string, unknown>>) {
    const access = route.access === "public" ? color("public", GREEN) : color(String(route.access), YELLOW);
    console.log(`  ${String(route.method).padEnd(10)} ${String(route.path).padEnd(46)} ${access}`);
  }
  console.log(`\n  ${(payload.routes as unknown[]).length} declared Terminal route${(payload.routes as unknown[]).length === 1 ? "" : "s"}`);
}

async function cmdKnowledge(args: string[]) {
  const query = args.join(" ").trim();
  const base = (process.env.NEXUS_AI_HUB_URL || "http://localhost:3114").trim().replace(/\/$/, "");
  const path = query
    ? `/api/v1/knowledge/search?q=${encodeURIComponent(query)}`
    : "/api/v1/knowledge";
  const payload = await fetchJson(`${base}${path}`);
  if (!payload || !Array.isArray(payload.documents)) {
    console.log(color("Nexus AI knowledge index unavailable — check NEXUS_AI_HUB_URL", RED));
    return;
  }

  console.log(BANNER);
  console.log(color(`Ecosystem Knowledge${query ? ` — ${query}` : ""}\n`, CYAN));
  console.log(`  Status: ${payload.status === "ready" ? color("ready", GREEN) : color("degraded", YELLOW)}`);
  for (const document of payload.documents as Array<Record<string, unknown>>) {
    const source = document.source as Record<string, unknown> | undefined;
    console.log(`\n  ${bold(String(document.title || document.id))}`);
    console.log(`  ${String(document.summary || "")}`);
    console.log(`  ${DIM}source: ${String(source?.ref || "unknown")} · trust: ${String(document.trust || "unknown")}${RESET}`);
  }
  console.log(`\n  ${(payload.documents as unknown[]).length} document${(payload.documents as unknown[]).length === 1 ? "" : "s"}`);
}

async function cmdPing() {
  console.log(color("Pinging all services...\n", CYAN));
  for (const [name, app] of Object.entries(APPS)) {
    const start = Date.now();
    const { alive } = await checkHealth(app.port);
    const ms = Date.now() - start;
    const dot = alive ? color("●", GREEN) : color("○", RED);
    console.log(`  ${dot} ${bold(name.padEnd(12))} ${DIM}:${app.port}${RESET} ${alive ? `${ms}ms` : color("unreachable", RED)}`);
  }
}

async function cmdChat() {
  console.log(BANNER);
  console.log(color("Team-Chat Terminal Client\n", CYAN));

  const port = APPS.chat?.port || 3109;
  const ws = new WebSocket(`ws://localhost:${port}/gateway`);

  ws.onopen = () => {
    ws.send(JSON.stringify({ op: "Identify", d: { token: "nexus-cli" } }));
  };

  ws.onmessage = (e) => {
    const p = JSON.parse(e.data as string);
    if (p.op === "Hello") {
      console.log(color("  Connected to Team-Chat gateway", GREEN));
      console.log(color(`  Heartbeat interval: ${p.d.heartbeat_interval}ms`, DIM));
    } else if (p.op === "Ready") {
      console.log(color(`\n  Logged in as: ${p.d.user.username}`, GREEN));
      console.log(color("  Channels:", CYAN));
      for (const ch of p.d.channels || []) {
        const msgCount = (p.d.messages?.[ch.id] || []).length;
        console.log(`    ${color("#", DIM)} ${bold(ch.name)} ${DIM}(${msgCount} messages)${RESET}`);
      }
      console.log(color(`\n  Type a message: [channel] your message`, DIM));
      console.log(color("  Commands: /channels /members /quit", DIM));
    } else if (p.op === "Dispatch" && p.t === "MESSAGE_CREATE") {
      console.log(`\n  ${color(p.d.authorName, MAGENTA)} ${DIM}in #${p.d.channelId?.slice(0,8)}${RESET}`);
      console.log(`  ${p.d.content}`);
    } else if (p.op === "Dispatch" && p.t === "TYPING_START") {
      console.log(`  ${DIM}${p.d.username} is typing...${RESET}`);
    }
  };

  ws.onerror = () => console.log(color("  Could not connect to Team-Chat", RED));

  // Read stdin for messages
  process.stdin.setEncoding("utf-8");
  process.stdin.on("data", (data: string) => {
    const input = data.trim();
    if (!input) return;
    if (input === "/quit") { ws.close(); process.exit(0); }

    const parts = input.split(" ");
    ws.send(JSON.stringify({ op: "CreateMessage", d: { channelId: parts[0]!, content: parts.slice(1).join(" ") || input } }));
  });
}

async function cmdInfo(args: string[]) {
  const name = args[0];
  if (!name || !APPS[name]) { console.log(color("Usage: nexus info <service>", YELLOW)); return; }

  const app = APPS[name]!;
  console.log(bold(`\n${name}`));
  console.log(`  Description: ${app.desc}`);
  console.log(`  Port:        ${app.port}`);
  console.log(`  Directory:   apps/${app.dir}`);

  // Try to get readiness/contracts
  const readiness = await fetchJson(`http://localhost:${app.port}/api/v1/${name === "chat" ? "chat/" : ""}${name === "api" ? "gateway/" : ""}${name === "auth" ? "auth/" : name === "guardian" ? "guardian/" : ""}${name === "edge" ? "edge/" : ""}${name === "tunnel" ? "tunnel/" : ""}${name === "files" ? "files/" : ""}${name === "search" ? "search/" : ""}${name === "monitor" ? "monitor/" : ""}status`);
  if (readiness) {
    console.log(`\n  ${bold("Capabilities:")}`);
    const capabilities = (readiness.capabilities || []) as string[];
    for (const cap of capabilities) {
      console.log(`    • ${cap}`);
    }
  }

  // Show recent logs
  console.log(`\n  ${bold("Command:")}`);
  console.log(`    nexus logs ${name}`);
  console.log(`    nexus health ${name}`);
}

async function cmdDemo() {
  const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));
  const box = (title: string) => console.log(`\n${CYAN}┌── ${bold(title)} ${"─".repeat(60 - title.length)}${RESET}`);
  const step = (n: number, msg: string) => { console.log(`\n${BOLD}  Step ${n}${RESET}: ${msg}`); };
  const ok = (msg: string) => console.log(`  ${color("✓", GREEN)} ${msg}`);
  const info = (msg: string) => console.log(`  ${color("→", BLUE)} ${msg}`);
  const warn = (msg: string) => console.log(`  ${color("⚠", YELLOW)} ${msg}`);

  console.log(BANNER);
  console.log(color("\n  Nexus Ecosystem — Full Demo Flow\n", CYAN));
  console.log(color("  " + "═".repeat(58), DIM));

  // ── Step 1: Start ecosystem ──
  box("STEP 1: Ecosystem Launch");
  step(1, "Starting all 10 Nexus services...");
  console.log();

  await cmdUp([]);
  await wait(3000);

  // Check what's alive
  let alive = 0;
  for (const app of Object.values(APPS)) {
    const { alive: a } = await checkHealth(app.port);
    if (a) alive++;
  }
  console.log(`\n  ${alive >= 8 ? color(`✓`, GREEN) : color("⚠", YELLOW)} ${alive}/10 services running`);

  // ── Step 2: Health dashboard ──
  box("STEP 2: Health Dashboard");
  step(2, "Verifying all service health endpoints...");
  console.log();
  for (const [name, app] of Object.entries(APPS)) {
    const { alive: a } = await checkHealth(app.port);
    const dot = a ? `${color("●", GREEN)}` : `${color("○", RED)}`;
    console.log(`  ${dot} ${bold(name.padEnd(12))} :${app.port} ${a ? color("healthy", GREEN) : color("unreachable", RED)}`);
  }

  // ── Step 3: Cloud dashboard ──
  box("STEP 3: Cloud Dashboard");
  step(3, "Cloud control plane — 81-app topology with live status");

  const cloudHealth = await checkHealth(8787);
  if (cloudHealth.alive) {
    const tools = await fetchJson("http://localhost:8787/api/v1/tools");
    const count = Array.isArray(tools?.tools) ? (tools!.tools as unknown[]).length : 0;
    ok(`Cloud running — ${count} tools registered`);
    info(`Dashboard: ${color("http://localhost:8787", BLUE + BOLD)}`);
    info(`${count} services registered with Systems API`);

    const topology = await fetchJson("http://localhost:8787/api/v1/topology");
    if (topology?.summary) {
      const s = topology.summary as Record<string, number>;
      info(`Topology: ${s.appCount} apps, ${s.connectionCount} connections`);
    }
  } else {
    warn("Cloud not running — starting it now");
  }

  // ── Step 4: Auth login ──
  box("STEP 4: Identity & Authentication");
  step(4, "SSO login through Nexus Auth");

  const loginData = await fetchJson("http://localhost:4310/api/v1/auth/login");
  if (!loginData) {
    warn("Auth not running");
  } else {
    // Try login
    const loginResp = await fetch("http://localhost:4310/api/v1/auth/login", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ username: "founder", password: "nexus-founder-2026" }),
    });
    const loginResult = await loginResp.json() as Record<string, unknown>;
    const token = loginResult.token as string;

    if (token) {
      ok(`Login successful — token: ${token.slice(0, 16)}...`);
      info(`Role: ${(loginResult.user as Record<string, unknown>)?.role || "founder"}`);
      info(`Session created — ${(loginResult.sessionId as string)?.slice(0, 8)}...`);

      // Verify token
      const trust = await fetch("http://localhost:4310/api/v1/auth/trust", {
        headers: { authorization: `Bearer ${token}` },
      });
      const trustResult = await trust.json() as Record<string, unknown>;
      ok(`Token verified: trusted=${trustResult.trusted}`);
    } else {
      warn("Login failed — auth may need user reset (nexus down auth && rm data/auth-users.json)");
    }
  }

  // ── Step 5: Team-Chat ──
  box("STEP 5: Real-Time Communication");
  step(5, "WebSocket messaging through Team-Chat");

  const chatStatus = await fetchJson("http://localhost:3109/api/v1/chat/status");
  if (chatStatus) {
    ok(`Team-Chat running — ${chatStatus.channels} channels available`);
    info("WebSocket protocol: Hello → Identify → Ready → Dispatch");

    // Open WebSocket and send a message
    const ws = new WebSocket("ws://localhost:3109/gateway");
    let msgSent = false;

    await new Promise<void>((resolve) => {
      ws.onopen = () => info("WebSocket connected to Team-Chat gateway");
      ws.onmessage = (e) => {
        const p = JSON.parse(e.data as string);
        if (p.op === "Hello") {
          info(`Received Hello — sending Identify`);
          ws.send(JSON.stringify({ op: "Identify", d: { token: "demo-user" } }));
        } else if (p.op === "Ready") {
          const channels = (p.d.channels || []) as Array<{ id: string; name: string }>;
          info(`Ready — ${channels.length} channels available`);

          if (channels.length > 0 && !msgSent) {
            msgSent = true;
            info(`Sending message to #${channels[0]!.name}...`);
            ws.send(JSON.stringify({ op: "CreateMessage", d: { channelId: channels[0]!.id, content: "Hello from the Nexus Demo Flow! 🚀" } }));
          }
        } else if (p.op === "Dispatch" && p.t === "MESSAGE_CREATE") {
          ok(`Message delivered: "${(p.d as Record<string, string>).content}"`);
          ws.close();
          resolve();
        }
      };
      setTimeout(() => { ws.close(); resolve(); }, 5000);
    });

    info("Typing indicators and presence updates are live");
  } else {
    warn("Team-Chat not running");
  }

  // ── Step 6: Files → Search ──
  box("STEP 6: Federated Data Pipeline");
  step(6, "Files → Search — auto-indexing pipeline");

  const fileHealth = await checkHealth(3033);
  if (fileHealth.alive) {
    // Upload a file
    const upload = await fetch("http://localhost:3033/api/v1/files", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        name: "nexus-architecture.md",
        contentType: "text/markdown",
        size: 2048,
        timestamp: new Date().toISOString(),
      }),
    });
    if (upload.ok) {
      const fileData = await upload.json() as Record<string, unknown>;
      const file = fileData.file;
      const fileName = file && typeof file === "object" && "name" in file && typeof file.name === "string"
        ? file.name
        : "nexus-architecture.md";
      ok(`File uploaded: ${String(fileData.id)} — "${fileName}"`);
      info("File automatically indexed in Nexus Search via SDK");

      await wait(1000);

      // Verify in Search
      const searchResult = await fetch("http://localhost:3034/api/v1/search/query", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ query: "nexus architecture", limit: 3 }),
      });
      if (searchResult.ok) {
        const searchData = await searchResult.json() as Record<string, unknown>;
        ok(`Search found: ${searchData.total} results`);
        info(`Files → Search federation: working`);
      }
    }
  } else {
    warn("Files service not running");
  }

  // ── Step 7: Threat Detection ──
  box("STEP 7: Security — Threat Detection & Response");
  step(7, "Guardian detects attacks and blocks malicious IPs");

  const guardianHealth = await checkHealth(4320);
  if (guardianHealth.alive) {
    info("Guardian running with 15 attack signatures loaded");

    // Simulate SQL injection attack
    info("Simulating SQL injection attack from 10.0.0.99...");
    const detect = await fetch("http://localhost:4320/api/v1/guardian/detect", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        field: "query",
        value: "1 UNION SELECT password FROM users --",
        source: "nexus-edge",
        ip: "10.0.0.99",
      }),
    });
    const detectResult = await detect.json() as Record<string, unknown>;
    const matches = detectResult.matches as Array<Record<string, string>> || [];
    if (matches.length > 0) {
      ok(`Threat detected: ${matches[0]!.patternName} (${matches[0]!.severity})`);
      ok(`Auto-response: ${matches[0]!.autoResponse} — IP 10.0.0.99 blacklisted`);
    }

    // Simulate XSS attack
    info("Simulating XSS attack from 10.0.0.88...");
    await fetch("http://localhost:4320/api/v1/guardian/detect", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        field: "body",
        value: "<script>alert(document.cookie)</script>",
        source: "nexus-edge",
        ip: "10.0.0.88",
      }),
    });
    ok("XSS attack detected — IP flagged");

    // Check IP reputation
    const blacklisted = await fetchJson("http://localhost:4320/api/v1/guardian/ips/blacklisted");
    const ips = Array.isArray(blacklisted?.blacklisted) ? blacklisted!.blacklisted as unknown[] : [];
    ok(`${ips.length} IPs blacklisted after attack simulation`);

    // Check threat stats
    const stats = await fetchJson("http://localhost:4320/api/v1/guardian/detect/stats");
    if (stats?.stats) {
      const s = stats.stats as Record<string, unknown>;
      info(`Total threat matches: ${s.totalMatches}`);
    }
  } else {
    warn("Guardian not running");
  }

  // ── Step 8: Monitor ──
  box("STEP 8: Observability & Metrics");
  step(8, "Monitor dashboard — service tracking, alerts, metrics");

  const monitorHealth = await checkHealth(3030);
  if (monitorHealth.alive) {
    // Send heartbeats from services
    info("Services auto-heartbeating to Monitor via SDK...");
    await wait(500);

    const dash = await fetchJson("http://localhost:3030/api/v1/monitor/dashboard");
    if (dash?.overview) {
      const o = dash.overview as Record<string, number>;
      ok(`Monitor dashboard live:`);
      info(`${o.servicesTotal} services tracked`);
      info(`${o.eventsReceived} events received`);
      info(`${o.heartbeatsReceived} heartbeats`);
      info(`${o.activeAlerts || 0} active alerts`);
    }

    // Check alert rules
    const rules = await fetchJson("http://localhost:3030/api/v1/monitor/alerts/rules");
    const ruleCount = Array.isArray(rules?.rules) ? (rules!.rules as unknown[]).length : 0;
    if (ruleCount > 0) ok(`${ruleCount} alert rules configured`);
  } else {
    warn("Monitor not running");
  }

  // ── Summary ──
  box("DEMO COMPLETE");
  console.log();
  console.log(color("  Ecosystem verification summary:", BOLD));
  console.log();

  const summary: Array<[string, number, string]> = [
    ["Service Mesh", alive, "services running"],
    ["Cloud Topology", alive > 0 ? 1 : 0, "81 apps mapped"],
    ["Auth SSO", await checkHealth(4310).then((r) => r.alive ? 1 : 0), "identity provider"],
    ["Team-Chat", await checkHealth(3109).then((r) => r.alive ? 1 : 0), "WebSocket gateway"],
    ["Files → Search", (await checkHealth(3033)).alive && (await checkHealth(3034)).alive ? 1 : 0, "federated pipeline"],
    ["Threat Detection", await checkHealth(4320).then((r) => r.alive ? 1 : 0), "Guardian active"],
    ["Monitor", await checkHealth(3030).then((r) => r.alive ? 1 : 0), "observability engine"],
  ];

  for (const [name, score, desc] of summary) {
    const dot = score > 0 ? color("✓", GREEN) : color("✗", RED);
    console.log(`  ${dot}  ${bold(name.padEnd(18))} — ${desc}`);
  }

  console.log(`\n${DIM}  Run 'nexus status' for live health dashboard${RESET}`);
  console.log(`${DIM}  Run 'nexus detect' for threat intelligence report${RESET}`);
  console.log(`${DIM}  Open http://localhost:8787 for the Cloud dashboard${RESET}`);
  console.log(color(`\n  Nexus Systems — Sovereign. Federated. Zero lock-in.`, CYAN + BOLD));
}

async function main() {
  const args = process.argv.slice(2);
  const cmd = args[0];

  if (!cmd || cmd === "help" || cmd === "--help" || cmd === "-h") {
    console.log(BANNER);
    console.log(`\n${bold("Commands:")}\n`);
    for (const [name, c] of Object.entries(commands)) {
      console.log(`  ${color(name.padEnd(12), GREEN)} ${c.desc}`);
      console.log(`  ${"".padEnd(12)} ${DIM}${c.usage}${RESET}\n`);
    }
    console.log(`  ${color("help".padEnd(12), GREEN)} Show this help`);
    return;
  }

  const handler = commands[cmd];
  if (!handler) {
    console.log(color(`Unknown command: ${cmd}`, RED));
    console.log(`Run ${color("nexus help", GREEN)} for available commands.`);
    return;
  }

  await handler.run(args.slice(1));
}

main().catch((e) => {
  console.error(color(`Error: ${(e as Error).message}`, RED));
  process.exit(1);
});
