// ─── nx-factory create — Scaffold a new Nexus app ─────────────────────────

import { existsSync } from "node:fs";
import { mkdir, writeFile } from "node:fs/promises";
import { dirname, join } from "node:path";

// ── Port registry ──────────────────────────────────────────────────────────
// Auto-assigns ports in the 303x-309x range, skipping already-taken ports.

const KNOWN_PORTS: Record<string, number> = {
  "nexus-monitor": 3030,
  "nexus-auth": 3031,
  "nexus-api": 3032,
  "nexus-files": 3033,
  "nexus-search": 3034,
  "nexus-tasks": 3035,
  "nexus-calendar": 3036,
  "nexus-notes": 3037,
  "nexus-email": 3038,
  "nexus-chat": 3039,
};

function suggestPort(appName: string): number {
  const known = KNOWN_PORTS[appName];
  if (known !== undefined) return known;

  // Hash the app name to get a deterministic port in the 3040-3099 range
  let hash = 0;
  for (let i = 0; i < appName.length; i++) {
    hash = (hash * 31 + appName.charCodeAt(i)) & 0xffffffff;
  }
  return 3040 + (Math.abs(hash) % 60);
}

function escapeJson(str: string): string {
  return str.replace(/\\/g, "\\\\").replace(/"/g, '\\"');
}

// ── Template engine ────────────────────────────────────────────────────────
// Simple {{PLACEHOLDER}} replacement — zero dependencies.

interface TemplateVars {
  APP_NAME: string;
  APP_TITLE: string;
  APP_DESCRIPTION: string;
  APP_PORT: string;
}

function render(template: string, vars: TemplateVars): string {
  let result = template;
  for (const [key, value] of Object.entries(vars)) {
    result = result.replaceAll(`{{${key}}}`, value);
  }
  return result;
}

// ── File descriptor: source template → destination path ───────────────────

interface FileDescriptor {
  template: string; // path relative to templates/ dir
  dest: string; // path relative to app root
}

const FILES: FileDescriptor[] = [
  { template: "_package.json", dest: "package.json" },
  { template: "_tsconfig.json", dest: "tsconfig.json" },
  { template: "_biome.json", dest: "biome.json" },
  { template: "_index.ts", dest: "src/index.ts" },
  { template: "_test.ts", dest: "tests/server.test.ts" },
  { template: "_README.md", dest: "README.md" },
  { template: "_AGENTS.md", dest: "AGENTS.md" },
];

// ── Command ────────────────────────────────────────────────────────────────

export interface CreateOptions {
  port?: string | undefined;
  description?: string | undefined;
}

export async function createApp(appName: string, options: CreateOptions = {}): Promise<void> {
  // Find the monorepo root — walk up until we find apps/ and packages/
  let root = process.cwd();
  // If we're inside a packages/nexus-factory directory, walk up to monorepo root
  while (!(existsSync(join(root, "apps")) && existsSync(join(root, "packages"))) && root !== "/") {
    root = dirname(root);
  }

  if (!existsSync(join(root, "apps")) || !existsSync(join(root, "packages"))) {
    process.stderr.write(
      "Error: could not find monorepo root (need apps/ and packages/ directories).\n" +
        "Run nx-factory from inside a Nexus monorepo or a directory with apps/ and packages/.\n",
    );
    process.exit(1);
  }

  const appDir = join(root, "apps", appName);

  if (existsSync(appDir)) {
    process.stderr.write(`Error: app directory already exists: ${appDir}\n`);
    process.exit(1);
  }

  const port = options.port ?? String(suggestPort(appName));
  const description = options.description ?? `${appName} — Nexus service`;

  // Derive title from app name: nexus-warehouse → Nexus Warehouse
  const title = appName
    .split("-")
    .map((part) => (part[0]?.toUpperCase() ?? "") + part.slice(1))
    .join(" ");

  const vars: TemplateVars = {
    APP_NAME: appName,
    APP_TITLE: title,
    APP_DESCRIPTION: escapeJson(description),
    APP_PORT: port,
  };

  // Read template directory
  const templateDir = join(dirname(new URL(import.meta.url).pathname), "..", "templates");

  // Create directory structure
  await mkdir(join(appDir, "src"), { recursive: true });
  await mkdir(join(appDir, "tests"), { recursive: true });
  await mkdir(join(appDir, "docs"), { recursive: true });

  // Render and write each file
  for (const file of FILES) {
    const templatePath = join(templateDir, file.template);
    const destPath = join(appDir, file.dest);

    let templateContent: string;
    try {
      templateContent = await Bun.file(templatePath).text();
    } catch {
      process.stderr.write(`Warning: template not found: ${file.template}, skipping\n`);
      continue;
    }

    // .json templates: render then re-parse to validate JSON
    if (file.template.endsWith(".json")) {
      const rendered = render(templateContent, vars);
      try {
        // Validate JSON by parsing and re-stringifying
        const parsed = JSON.parse(rendered);
        await writeFile(destPath, JSON.stringify(parsed, null, 2) + "\n", "utf8");
      } catch (err) {
        process.stderr.write(
          `Error: invalid JSON after rendering ${file.template}: ${err instanceof Error ? err.message : String(err)}\n`,
        );
        process.exit(1);
      }
    } else {
      const rendered = render(templateContent, vars);
      await writeFile(destPath, rendered, "utf8");
    }
  }

  process.stdout.write(
    JSON.stringify({
      level: "info",
      message: "app created",
      name: appName,
      path: appDir,
      port,
      files: FILES.length,
    }) + "\n",
  );

  // Print next steps
  process.stderr.write(`
✅ ${title} scaffolded at ${appDir}

Next steps:
  cd apps/${appName}
  bun install
  bun dev          # start with hot reload on port ${port}
  bun test         # run the smoke test
  bun run check    # typecheck + lint

Start adding routes in src/index.ts!
`);
}
