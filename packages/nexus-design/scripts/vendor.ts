import { copyFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

/**
 * Where the generated token CSS is copied to.
 *
 * Chat and Hosting are separate git repositories, so they cannot import from
 * this package — a standalone clone (or Hosting's Docker build, whose context
 * is its own repo) would not resolve the path, and both must keep building on
 * their own. A vendored copy plus a drift test is the trade: duplication that
 * cannot rot silently.
 *
 * Cloud used to be a target; it retired its frontend, so there is nothing
 * left there to theme.
 */
export function vendorTargets(): { repo: string; dest: string }[] {
  return [
    { repo: "apps/Nexus", dest: "apps/Nexus/packages/nexus-web/src/nexus-tokens.css" },
    {
      repo: "apps/Nexus-Hosting",
      dest: "apps/Nexus-Hosting/artifacts/federated-hosting/src/nexus-tokens.css",
    },
  ];
}

export function vendorAll(root: string): void {
  const src = join(root, "packages/nexus-design/dist/nexus-tokens.css");
  for (const t of vendorTargets()) {
    const dest = join(root, t.dest);
    mkdirSync(dirname(dest), { recursive: true });
    copyFileSync(src, dest);
  }
}

// CLI entry so `bun run scripts/vendor.ts` works standalone (chained after
// `build` in the "vendor" package.json script, which regenerates dist/ first).
if (import.meta.main) {
  const here = dirname(fileURLToPath(import.meta.url));
  const root = join(here, "..", "..", "..");
  vendorAll(root);
  for (const t of vendorTargets()) {
    console.log(`[nexus-design] vendored -> ${t.dest}`);
  }
}
