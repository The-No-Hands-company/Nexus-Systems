export interface TemplateVars {
  name: string;
  service: string;
  displayName: string;
  envPrefix: string;
  port: string;
  description: string;
  domain: string;
  domainPlural: string;
  domainField: string;
  caps: string;
}

function pluralize(s: string): string {
  if (s.endsWith("s") || s.endsWith("x") || s.endsWith("z") || s.endsWith("ch") || s.endsWith("sh")) return s + "es";
  if (s.endsWith("y") && !["a", "e", "i", "o", "u"].includes(s[s.length - 2] ?? "")) return s.slice(0, -1) + "ies";
  return s + "s";
}

export function deriveVars(appName: string, port: number, description: string): TemplateVars {
  const name = appName;
  const service = `nexus-${name.toLowerCase()}`;
  const displayName = `Nexus-${name}`;
  const envPrefix = `NEXUS_${name.replace(/-/g, "_").toUpperCase()}`;
  const p = String(port);
  const desc = description || `${displayName} Nexus Systems app`;
  const domain = name.toLowerCase().replace(/-/g, "");
  const domainPlural = pluralize(domain);
  const domainField = deriveDomainField(domain);
  const caps = `["${domain}"]`;

  return { name, service, displayName, envPrefix, port: p, description: desc, domain, domainPlural, domainField, caps };
}

function deriveDomainField(domain: string): string {
  const map: Record<string, string> = {
    account: "email",
    code: "url",
    file: "path",
    store: "sku",
    health: "record",
    game: "genre",
    music: "track",
    video: "format",
    photo: "format",
    book: "isbn",
    learn: "subject",
    notes: "category",
    tasks: "priority",
    calendar: "date",
    contacts: "phone",
    finance: "currency",
    billing: "currency",
    invoice: "amount",
    nutrition: "calories",
    fitness: "duration",
    spend: "amount",
    sales: "amount",
    inventory: "sku",
    warehouse: "location",
    shipping: "tracking",
    analytics: "metric",
    insights: "metric",
    monitor: "metric",
    search: "query",
    social: "platform",
    chat: "channel",
    meet: "room",
    email: "address",
    survey: "rating",
    form: "schema",
    tunnel: "host",
    deploy: "target",
  };
  return map[domain] ?? "type";
}

export function packageJson(v: TemplateVars): string {
  return JSON.stringify({
    name: v.service,
    private: true,
    version: "0.1.0",
    type: "module",
    description: v.description,
    scripts: {
      dev: "bun run src/index.ts",
      start: "bun run src/index.ts",
      check: "bunx tsc --noEmit",
      test: "bun test",
      lint: "bunx biome check src tests",
      format: "bunx biome check --write src tests",
    },
    devDependencies: {
      "@biomejs/biome": "^1.9.4",
      "bun-types": "^1.3.11",
      typescript: "^5.8.3",
    },
  }, null, 2) + "\n";
}

export function tsconfig(): string {
  return JSON.stringify({
    compilerOptions: {
      target: "ESNext",
      lib: ["ESNext"],
      module: "ESNext",
      moduleResolution: "bundler",
      types: ["bun-types"],
      strict: true,
      noUncheckedIndexedAccess: true,
      exactOptionalPropertyTypes: true,
      noImplicitReturns: true,
      noFallthroughCasesInSwitch: true,
      skipLibCheck: true,
    },
    include: ["src", "tests"],
  }, null, 2) + "\n";
}

export function biome(): string {
  return JSON.stringify({
    $schema: "https://biomejs.dev/schemas/1.9.4/schema.json",
    formatter: {
      indentStyle: "space",
      lineWidth: 100,
    },
    linter: {
      enabled: true,
      rules: {
        recommended: true,
      },
    },
  }, null, 2) + "\n";
}

export function readme(v: TemplateVars): string {
  return `# ${v.displayName}

## Purpose

${v.description}

## Quick Start

\`\`\`bash
bun install
bun run dev
\`\`\`

## Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Health check |
| GET | /api/v1/status | Service status and contracts |

## Configuration

| Variable | Default | Purpose |
|----------|---------|---------|
| PORT | ${v.port} | Listen port |
| NEXUS_CLOUD_URL | http://localhost:8787 | Cloud control plane |
| NEXUS_CLOUD_API_KEY | (none) | Cloud API key |
| \`${v.envPrefix}_ENABLE_CLOUD_INTEGRATION\` | true | Enable/disable cloud heartbeat |
`;
}

export function agents(v: TemplateVars): string {
  return `# AGENTS

Scope: ${v.displayName} workspace.

Guidelines:
- Keep changes minimal and reversible.
- Prioritize API contract compatibility with Nexus-Cloud Systems API.
- Add tests with each behavior change.

Standards Enforcement:
- Follow ../docs/ENGINEERING_STANDARDS.md for runtime, typing, API, testing, and security requirements.
- Keep this service on Bun + strict TypeScript.
- Required validation before handoff: bun run check && bun test.
`;
}

export function docsReadme(v: TemplateVars): string {
  return `# ${v.displayName} Docs

TODO: Add architecture, API contract, and rollout plan.
`;
}

export function srcEngine(v: TemplateVars): string {
  const plural = v.domainPlural;
  return `import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface ${v.name} {
  id: string;
  name: string;
  description: string;
  ${v.domainField}: string;
  createdAt: string;
  updatedAt: string;
}

export class ${v.name}Engine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS ${plural} (id TEXT PRIMARY KEY, name TEXT, description TEXT, ${v.domainField} TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  create${v.name}(name: string, ${v.domainField} = ""): ${v.name} {
    const d: ${v.name} = {
      id: randomUUID(),
      name,
      description: "",
      ${v.domainField}: ${v.domainField},
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO ${plural} VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.${v.domainField}, d.createdAt, d.updatedAt);
    return d;
  }

  list${v.name}s(): ${v.name}[] {
    return this.db.prepare("SELECT * FROM ${plural} ORDER BY updated_at DESC").all() as ${v.name}[];
  }

  get${v.name}(id: string): ${v.name} | undefined {
    return this.db.prepare("SELECT * FROM ${plural} WHERE id = ?").get(id) as ${v.name} | undefined;
  }
}
`;
}

export function srcServer(v: TemplateVars): string {
  return `import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { ${v.name}Engine } from "./${v.domain}-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "x-request-id": randomUUID(),
    },
  });
}

export function createServer() {
  const port = Number(process.env.PORT || "${v.port}");
  const baseUrl =
    process.env.${v.envPrefix}_BASE_URL || \`http://localhost:\${port}\`;
  const startedAt = Date.now();
  const engine = new ${v.name}Engine("data/${v.service}.sqlite");

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname || "";

      if (req.method === "GET" && p === "/health")
        return json({
          service: "${v.service}",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
        });

      if (req.method === "GET" && p === "/api/v1/status")
        return json(
          {
            service: "${v.service}",
            status: "ready",
            capabilities: ${v.caps},
            cloudIntegration: {
              enabled:
                (process.env["${v.envPrefix}_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
          },
          200,
        );

      if (req.method === "GET" && p === "/api/v1/${v.domain}/${v.domainPlural}")
        return json(engine.list${v.name}s());

      if (req.method === "POST" && p === "/api/v1/${v.domain}/${v.domainPlural}") {
        const b = (await req.json().catch(() => ({}))) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.create${v.name}(b.name, b.${v.domainField}), 201);
      }

      const gm = p.match(/^\\/api\\/v1\\/${v.domain}\\/${v.domainPlural}\\/([^/]+)$/);
      if (req.method === "GET" && gm) {
        const entity = engine.get${v.name}(gm[1]!);
        return entity ? json(entity) : json({ error: "not found" }, 404);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(\`[${v.service}] Listening on port \${server.port}\`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, close: () => { stopHeartbeat(); server.stop(); } };
}
`;
}

export function srcContracts(v: TemplateVars): string {
  return `export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  metadata: Record<string, unknown>;
};

export function buildSystemsApiRegistrationPayload(
  baseUrl: string,
): SystemsApiRegistrationPayload {
  return {
    id: "${v.service}",
    name: "${v.displayName}",
    description: "${v.description}",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ${v.caps},
    metadata: {
      version: "v1",
      defaultPort: ${v.port},
    },
  };
}
`;
}

export function srcCloud(v: TemplateVars): string {
  return `import { buildSystemsApiRegistrationPayload } from "./contracts";

function cloudBaseUrl(): string {
  return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787")
    .trim()
    .replace(/\\/$/, "");
}

function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    accept: "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY
      ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY }
      : {}),
  };
}

function hbMs(): number {
  return Math.max(
    5000,
    Number(
      process.env["${v.envPrefix}_CLOUD_HEARTBEAT_INTERVAL_MS"] || "30000",
    ),
  );
}

function enabled(): boolean {
  return (
    (process.env["${v.envPrefix}_ENABLE_CLOUD_INTEGRATION"] || "true")
      .trim()
      .toLowerCase() !== "false"
  );
}

export async function registerWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(\`\${cloudBaseUrl()}/api/v1/tools\`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(buildSystemsApiRegistrationPayload(baseUrl)),
  });
  if (!r.ok) throw new Error(\`${v.displayName} registration failed: \${r.status}\`);
}

export async function heartbeatWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(
    \`\${cloudBaseUrl()}/api/v1/tools/\${encodeURIComponent("${v.service}")}/heartbeat\`,
    {
      method: "POST",
      headers: cloudHeaders(),
      body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
    },
  );
  if (!r.ok)
    throw new Error(\`${v.displayName} heartbeat failed: \${r.status}\`);
}

export function startHeartbeat(baseUrl: string): () => void {
  if (!enabled()) return () => {};

  registerWithCloud(baseUrl).catch((e) => {
    console.warn(
      \`[${v.service}] Cloud registration failed: \${(e as Error).message}\`,
    );
  });

  const timer = setInterval(() => {
    heartbeatWithCloud(baseUrl).catch((e) => {
      console.warn(
        \`[${v.service}] Cloud heartbeat failed: \${(e as Error).message}\`,
      );
    });
  }, hbMs());

  if (typeof timer.unref === "function") timer.unref();
  return () => clearInterval(timer);
}
`;
}

export function srcIndex(): string {
  return `import { createServer } from "./server";

const { close } = createServer();

process.on("SIGTERM", () => {
  close();
  process.exit(0);
});
process.on("SIGINT", () => {
  close();
  process.exit(0);
});
`;
}

export function testServer(v: TemplateVars): string {
  return `import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("${v.service}", () => {
  let base = "";
  let handle: ReturnType<typeof createServer>;

  beforeAll(async () => {
    handle = createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = \`http://127.0.0.1:\${handle.server.port}\`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(\`\${base}/health\`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("${v.service}");
    expect(body["status"]).toBe("ok");
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(\`\${base}/api/v1/status\`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body["service"]).toBe("${v.service}");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });
});
`;
}
