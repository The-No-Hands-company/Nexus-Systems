// ─── nx-factory check — Run typecheck + lint + test ────────────────────────

import { existsSync } from "node:fs";
import { dirname, join } from "node:path";

export async function runCheck(targetApp?: string): Promise<void> {
  // Find the monorepo root
  let root = process.cwd();
  while (!(existsSync(join(root, "apps")) && existsSync(join(root, "packages"))) && root !== "/") {
    root = dirname(root);
  }

  if (!existsSync(join(root, "apps")) || !existsSync(join(root, "packages"))) {
    process.stderr.write(
      "Error: could not find monorepo root (need apps/ and packages/ directories).\n",
    );
    process.exit(1);
  }

  const appsDir = join(root, "apps");

  // Determine which app directories to check
  let appDirs: string[];
  if (targetApp) {
    const appDir = join(appsDir, targetApp);
    if (!existsSync(appDir)) {
      process.stderr.write(`Error: app directory not found: ${appDir}\n`);
      process.exit(1);
    }
    appDirs = [appDir];
  } else {
    // Find all apps that have package.json (real apps, not shells)
    const entries = Array.from(
      new Bun.Glob("*/package.json").scanSync({ cwd: appsDir, absolute: false }),
    );
    appDirs = entries.map((e) => join(appsDir, dirname(e)));
  }

  if (appDirs.length === 0) {
    process.stderr.write("No apps found to check.\n");
    process.exit(0);
  }

  process.stderr.write(`Checking ${appDirs.length} app(s)...\n\n`);

  let failed = false;

  for (const appDir of appDirs) {
    const appName = appDir.split("/").pop() ?? appDir;

    process.stderr.write(`── ${appName} ──\n`);

    // typecheck
    const tcResult = Bun.spawnSync(["bun", "run", "typecheck"], {
      cwd: appDir,
      stdio: ["ignore", "pipe", "pipe"],
    });
    if (tcResult.exitCode === 0) {
      process.stderr.write("  ✅ typecheck\n");
    } else {
      failed = true;
      process.stderr.write("  ❌ typecheck failed\n");
      process.stderr.write(tcResult.stderr.toString());
    }

    // lint
    const lintResult = Bun.spawnSync(["bun", "run", "lint"], {
      cwd: appDir,
      stdio: ["ignore", "pipe", "pipe"],
    });
    if (lintResult.exitCode === 0) {
      process.stderr.write("  ✅ lint\n");
    } else {
      failed = true;
      process.stderr.write("  ❌ lint failed\n");
      process.stderr.write(lintResult.stderr.toString());
    }

    // test
    const testResult = Bun.spawnSync(["bun", "test"], {
      cwd: appDir,
      stdio: ["ignore", "pipe", "pipe"],
    });
    if (testResult.exitCode === 0) {
      process.stderr.write("  ✅ test\n");
    } else {
      failed = true;
      process.stderr.write("  ❌ test failed\n");
      process.stderr.write(testResult.stderr.toString().slice(-2000));
    }

    process.stderr.write("\n");
  }

  if (failed) {
    process.stderr.write("❌ Some checks failed.\n");
    process.exit(1);
  }

  process.stderr.write("✅ All checks passed.\n");
}
