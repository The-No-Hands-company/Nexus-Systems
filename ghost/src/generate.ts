import { mkdir, symlink, writeFile } from "node:fs/promises";
import { join } from "node:path";
import {
  agents,
  biome,
  deriveVars,
  docsReadme,
  packageJson,
  readme,
  srcCloud,
  srcContracts,
  srcEngine,
  srcIndex,
  srcServer,
  testServer,
  tsconfig,
  type TemplateVars,
} from "./templates";

const APPS_DIR = join(import.meta.dirname!, "..", "..", "apps");

interface ScaffoldOptions {
  name: string;
  port: number;
  description: string;
}

async function writeDir(base: string, path: string) {
  await mkdir(join(base, path), { recursive: true });
}

async function write(base: string, path: string, content: string) {
  const full = join(base, path);
  await writeFile(full, content, "utf-8");
  console.log(`  ${path}`);
}

export async function scaffold(opts: ScaffoldOptions): Promise<string> {
  const v: TemplateVars = deriveVars(opts.name, opts.port, opts.description);
  const appDir = join(APPS_DIR, v.displayName);

  console.log(`\nScaffolding ${v.displayName}...\n`);

  await mkdir(appDir, { recursive: true });

  await writeDir(appDir, "src");
  await writeDir(appDir, "tests");
  await writeDir(appDir, "docs");

  await write(appDir, "package.json", packageJson(v));
  await write(appDir, "tsconfig.json", tsconfig());
  await write(appDir, "biome.json", biome());
  await write(appDir, "README.md", readme(v));
  await write(appDir, "AGENTS.md", agents(v));
  await write(appDir, "docs/README.md", docsReadme(v));

  await write(appDir, `src/${v.domain}-engine.ts`, srcEngine(v));
  await write(appDir, "src/server.ts", srcServer(v));
  await write(appDir, "src/contracts.ts", srcContracts(v));
  await write(appDir, "src/cloud.ts", srcCloud(v));
  await write(appDir, "src/index.ts", srcIndex());

  await write(appDir, "src/.gitkeep", "");
  await write(appDir, "tests/.gitkeep", "");

  await write(appDir, "tests/server.test.ts", testServer(v));

  const linkPath = join(appDir, "data");
  try {
    await symlink("../../data", linkPath, "dir");
  } catch {
    console.warn(`  [warn] Could not create data symlink (might already exist)`);
  }

  // Install dependencies via bun
  try {
    console.log("  Installing dependencies...");
    Bun.spawnSync(["bun", "install"], { cwd: appDir, stdout: "pipe", stderr: "pipe" });
    console.log("  Dependencies installed.");
  } catch {
    console.warn("  [warn] Could not install dependencies (run `bun install` manually)");
  }

  console.log(`\nDone. ${v.displayName} scaffolded at ${appDir}\n`);
  return appDir;
}
