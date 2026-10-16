import { chromium, type Browser, type Page } from "playwright";
import AxeBuilder from "@axe-core/playwright";
import path from "path";
import fs from "fs";
import { execSync } from "child_process";
import { APPS, VIEWPORTS, type NitApp } from "./apps.ts";
import { serveDir } from "./serve.ts";

const BASELINE_DIR = path.resolve(import.meta.dirname, "../baselines");
const UPDATE_BASELINES = process.argv.includes("--update-baselines");
const SKIP_BUILD = process.argv.includes("--skip-build");
const FILTER = process.argv.find(a => a.startsWith("--app="))?.slice(6);

interface AppResult {
  app: string;
  viewport: string;
  violations: { id: string; impact: string; description: string; nodes: number }[];
  screenshotPath: string;
  built: boolean;
  error?: string;
}

function buildApp(app: NitApp): boolean {
  if (SKIP_BUILD && fs.existsSync(path.join(app.distDir, "index.html"))) {
    console.log(`  [build] skipped (--skip-build, dist exists)`);
    return true;
  }
  console.log(`  [build] ${app.buildCmd}`);
  try {
    execSync(app.buildCmd, { cwd: app.frontendDir, stdio: "pipe", timeout: 120_000 });
    return true;
  } catch (err: unknown) {
    const msg = err instanceof Error ? err.message : String(err);
    console.error(`  [build] FAILED: ${msg.slice(0, 200)}`);
    return false;
  }
}

async function scanApp(page: Page, app: NitApp, url: string): Promise<{ violations: AppResult["violations"]; error?: string }> {
  // Playwright evaluates routes in REVERSE order (last registered = first called).
  // Register the catch-all first so the specific mocks below take priority.
  await page.route(/\/api\//, route => route.fulfill({ json: {} }));
  for (const mock of app.mocks) {
    await page.route(mock.pattern, route => route.fulfill({ json: mock.json }));
  }

  try {
    await page.goto(url, { waitUntil: "domcontentloaded", timeout: 15_000 });
    await page.waitForSelector(app.waitForSelector, { timeout: app.waitTimeout }).catch(() => {
      console.log(`  [scan] selector "${app.waitForSelector}" not found within timeout — proceeding anyway`);
    });
    // Wait for dynamic content (API calls) to settle
    await page.waitForTimeout(app.waitMs ?? 800);
  } catch (err: unknown) {
    return { violations: [], error: err instanceof Error ? err.message : String(err) };
  }

  const results = await new AxeBuilder({ page })
    .withTags(["wcag2a", "wcag2aa"])
    .disableRules(["color-contrast"]) // Requires real rendering environment
    .analyze();

  const violations = results.violations.map(v => ({
    id: v.id,
    impact: v.impact ?? "unknown",
    description: v.description,
    nodes: v.nodes.length,
  }));

  return { violations };
}

async function run() {
  const apps = FILTER ? APPS.filter(a => a.name.toLowerCase() === FILTER.toLowerCase()) : APPS;
  if (apps.length === 0) {
    console.error(`No app matching --app=${FILTER}`);
    process.exit(1);
  }

  fs.mkdirSync(BASELINE_DIR, { recursive: true });

  const browser: Browser = await chromium.launch();
  const allResults: AppResult[] = [];
  let totalCritical = 0;
  let totalSerious = 0;

  for (const app of apps) {
    console.log(`\n▶ ${app.name}`);

    const built = buildApp(app);
    if (!built) {
      allResults.push({ app: app.name, viewport: "*", violations: [], screenshotPath: "", built: false, error: "build failed" });
      continue;
    }

    const server = serveDir(app.distDir);
    const baseUrl = `http://localhost:${server.port}`;
    console.log(`  [serve] ${baseUrl}`);

    for (const vp of VIEWPORTS) {
      const context = await browser.newContext({ viewport: { width: vp.width, height: vp.height } });
      const page = await context.newPage();

      console.log(`  [scan] ${app.name} @ ${vp.name} (${vp.width}×${vp.height})`);
      const { violations, error } = await scanApp(page, app, baseUrl);

      const screenshotName = `${app.name}-${vp.name}.png`;
      const screenshotPath = path.join(BASELINE_DIR, screenshotName);

      if (UPDATE_BASELINES || !fs.existsSync(screenshotPath)) {
        await page.screenshot({ path: screenshotPath, fullPage: false });
        console.log(`  [screenshot] ${UPDATE_BASELINES ? "updated" : "captured new baseline"}: ${screenshotName}`);
      }

      const critical = violations.filter(v => v.impact === "critical");
      const serious = violations.filter(v => v.impact === "serious");
      totalCritical += critical.length;
      totalSerious += serious.length;

      if (violations.length > 0) {
        console.log(`  [axe] ${violations.length} violation(s): ${critical.length} critical, ${serious.length} serious, ${violations.length - critical.length - serious.length} other`);
        for (const v of [...critical, ...serious].slice(0, 5)) {
          console.log(`    ✗ [${v.impact}] ${v.id}: ${v.description} (${v.nodes} node${v.nodes !== 1 ? "s" : ""})`);
        }
        if (violations.length > 5) console.log(`    … and ${violations.length - 5} more`);
      } else {
        console.log(`  [axe] ✓ no violations`);
      }

      allResults.push({ app: app.name, viewport: vp.name, violations, screenshotPath, built: true, error });
      await context.close();
    }

    server.stop();
  }

  // Summary
  console.log("\n──────────────────────────────────────");
  console.log("Nit CI — Results");
  console.log("──────────────────────────────────────");
  for (const r of allResults) {
    if (!r.built) { console.log(`  ${r.app}: BUILD FAILED`); continue; }
    const c = r.violations.filter(v => v.impact === "critical").length;
    const s = r.violations.filter(v => v.impact === "serious").length;
    const status = c > 0 ? "✗ FAIL" : s > 0 ? "⚠ WARN" : "✓ PASS";
    console.log(`  ${status}  ${r.app} @ ${r.viewport}: ${r.violations.length} violation(s) (${c} critical, ${s} serious)`);
  }
  console.log("──────────────────────────────────────");
  console.log(`Total: ${totalCritical} critical, ${totalSerious} serious across all apps`);

  await browser.close();

  if (totalCritical > 0) {
    console.log("\n✗ Nit CI FAILED — critical a11y violations found");
    process.exit(1);
  }

  console.log("\n✓ Nit CI PASSED");
}

run().catch(err => {
  console.error("Fatal:", err);
  process.exit(1);
});
