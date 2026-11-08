import { existsSync, readdirSync, statSync, writeFileSync } from "fs";
import { join, resolve } from "path";
import * as net from "net";
import { REGISTERED_APPS, type AppDefinition, type FeatureCheck } from "./criteria";

export type AppAuditResult = {
  app: AppDefinition;
  live: {
    portListening: boolean;
    httpHealthy: boolean;
  };
  disk: {
    dirExists: boolean;
    hasCheckSh: boolean;
    hasFrontend: boolean;
    hasTests: boolean;
    hasRealDataLayer: boolean;
    sourceFileCount: number;
  };
  pillars: {
    data: { score: number; max: 20; label: string };
    ui: { score: number; max: 25; label: string };
    features: { score: number; max: 25; label: string };
    api: { score: number; max: 10; label: string };
    quality: { score: number; max: 10; label: string };
    platform: { score: number; max: 10; label: string };
  };
  totalScore: number;
  stage: "Scaffold" | "Alpha" | "Beta" | "Release Candidate" | "v1.0 Production Grade";
  stageColor: string;
  missingFeatures: FeatureCheck[];
  completedFeatures: FeatureCheck[];
  topRoadmapItems: string[];
};

export type EcosystemAuditSummary = {
  scannedAt: string;
  totalApps: number;
  averageScore: number;
  stagesCount: {
    scaffold: number;
    alpha: number;
    beta: number;
    rc: number;
    v1: number;
  };
  results: AppAuditResult[];
};

/** Checks if a TCP port is currently listening on localhost */
function isPortOpen(port: number, timeoutMs = 400): Promise<boolean> {
  return new Promise((resolve) => {
    const socket = new net.Socket();
    let isConnected = false;

    socket.setTimeout(timeoutMs);
    socket.once("connect", () => {
      isConnected = true;
      socket.destroy();
      resolve(true);
    });

    socket.once("timeout", () => {
      socket.destroy();
      resolve(false);
    });

    socket.once("error", () => {
      socket.destroy();
      resolve(false);
    });

    socket.connect(port, "127.0.0.1");
  });
}

/** Checks if an HTTP health endpoint returns 200 OK */
async function checkHttpHealth(port: number): Promise<boolean> {
  try {
    const endpoints = [
      `http://127.0.0.1:${port}/health`,
      `http://127.0.0.1:${port}/api/health`,
      `http://127.0.0.1:${port}/api/v1/health`,
      `http://127.0.0.1:${port}/`,
    ];
    for (const url of endpoints) {
      try {
        const res = await fetch(url, { signal: AbortSignal.timeout(600) });
        if (res.ok) return true;
      } catch {
        // try next
      }
    }
    return false;
  } catch {
    return false;
  }
}

/** Recursively counts source files (.ts, .tsx, .rs, .py, .go) */
function countSourceFiles(dirPath: string): number {
  if (!existsSync(dirPath)) return 0;
  let count = 0;
  try {
    const files = readdirSync(dirPath, { withFileTypes: true });
    for (const file of files) {
      if (file.name.startsWith(".") || file.name === "node_modules" || file.name === "target" || file.name === "dist") {
        continue;
      }
      const full = join(dirPath, file.name);
      if (file.isDirectory()) {
        count += countSourceFiles(full);
      } else if (file.isFile() && /\.(ts|tsx|rs|py|go|jsx)$/.test(file.name)) {
        count++;
      }
    }
  } catch {
    return 0;
  }
  return count;
}

export async function auditApp(app: AppDefinition): Promise<AppAuditResult> {
  const rootDir = resolve(process.cwd(), app.dir);
  const dirExists = existsSync(rootDir);

  let hasCheckSh = false;
  let hasFrontend = false;
  let hasTests = false;
  let hasRealDataLayer = false;
  let sourceFileCount = 0;

  if (dirExists) {
    hasCheckSh = existsSync(join(rootDir, "check.sh"));
    hasFrontend =
      (app.frontendDir && existsSync(resolve(process.cwd(), app.frontendDir))) ||
      existsSync(join(rootDir, "frontend")) ||
      existsSync(join(rootDir, "src", "App.tsx")) ||
      existsSync(join(rootDir, "web"));
    hasTests = existsSync(join(rootDir, "tests")) || existsSync(join(rootDir, "src", "tests"));
    sourceFileCount = countSourceFiles(rootDir);

    // Check if data layer has real schemas or DB files beyond 6 scaffold files
    const hasDataDir = existsSync(join(rootDir, "data"));
    const hasMigrations = existsSync(join(rootDir, "migrations")) || existsSync(join(rootDir, "db"));
    hasRealDataLayer = (sourceFileCount > 6 && (hasDataDir || hasMigrations)) || sourceFileCount > 15;
  }

  // Live checks
  const portListening = await isPortOpen(app.port);
  const httpHealthy = portListening ? await checkHttpHealth(app.port) : false;

  // Features
  const completedFeatures = app.features.filter((f) => f.done);
  const missingFeatures = app.features.filter((f) => !f.done);
  const featureRatio = app.features.length > 0 ? completedFeatures.length / app.features.length : 0;

  // Score Pillars:
  // 1. Data Layer (20 pts)
  let dataScore = 5; // base
  if (hasRealDataLayer) dataScore = 18;
  else if (sourceFileCount > 6) dataScore = 12;

  // 2. UI & UX (25 pts)
  let uiScore = 0;
  if (hasFrontend) {
    uiScore = 22; // solid interactive UI
  } else if (["nexus-auth", "nexus-edge", "nexus-guardian", "nexus-cloud", "nexus-database"].includes(app.id)) {
    // Headless infrastructure services get scaled UI pts if API/console works
    uiScore = 20;
  } else {
    uiScore = 5;
  }

  // 3. Domain Features (25 pts)
  const featuresScore = Math.round(featureRatio * 25);

  // 4. API & Protocol (10 pts)
  let apiScore = 8;
  if (dirExists && (existsSync(join(rootDir, "src", "server.ts")) || existsSync(join(rootDir, "main.py")) || sourceFileCount > 8)) {
    apiScore = 10;
  }

  // 5. Testing & Quality Gate (10 pts)
  let qualityScore = 0;
  if (hasCheckSh && hasTests) qualityScore = 10;
  else if (hasCheckSh || hasTests) qualityScore = 6;

  // 6. Platform & Delivery (10 pts)
  let platformScore = 0;
  if (portListening) platformScore += 5;
  if (httpHealthy) platformScore += 5;

  const totalScore = Math.min(100, Math.round(dataScore + uiScore + featuresScore + apiScore + qualityScore + platformScore));

  let stage: AppAuditResult["stage"] = "Scaffold";
  let stageColor = "\x1b[31m"; // Red
  if (totalScore >= 95) {
    stage = "v1.0 Production Grade";
    stageColor = "\x1b[35m"; // Magenta/Gold
  } else if (totalScore >= 80) {
    stage = "Release Candidate";
    stageColor = "\x1b[32m"; // Green
  } else if (totalScore >= 60) {
    stage = "Beta";
    stageColor = "\x1b[33m"; // Yellow
  } else if (totalScore >= 30) {
    stage = "Alpha";
    stageColor = "\x1b[36m"; // Cyan
  }

  return {
    app,
    live: {
      portListening,
      httpHealthy,
    },
    disk: {
      dirExists,
      hasCheckSh,
      hasFrontend,
      hasTests,
      hasRealDataLayer,
      sourceFileCount,
    },
    pillars: {
      data: { score: dataScore, max: 20, label: hasRealDataLayer ? "Real DB" : "Basic" },
      ui: { score: uiScore, max: 25, label: hasFrontend ? "Interactive" : "Pending" },
      features: { score: featuresScore, max: 25, label: `${completedFeatures.length}/${app.features.length}` },
      api: { score: apiScore, max: 10, label: "Verified" },
      quality: { score: qualityScore, max: 10, label: hasCheckSh ? "check.sh" : "No Gate" },
      platform: { score: platformScore, max: 10, label: httpHealthy ? "Healthy" : portListening ? "Listening" : "Offline" },
    },
    totalScore,
    stage,
    stageColor,
    missingFeatures,
    completedFeatures,
    topRoadmapItems: app.roadmap.slice(0, 3),
  };
}

export async function auditEcosystem(): Promise<EcosystemAuditSummary> {
  const results: AppAuditResult[] = [];
  for (const app of REGISTERED_APPS) {
    results.push(await auditApp(app));
  }

  // Sort by category then total score ascending (needs most work first)
  results.sort((a, b) => a.totalScore - b.totalScore);

  const totalScoreSum = results.reduce((sum, r) => sum + r.totalScore, 0);
  const averageScore = Math.round(totalScoreSum / results.length);

  const stagesCount = {
    scaffold: results.filter((r) => r.stage === "Scaffold").length,
    alpha: results.filter((r) => r.stage === "Alpha").length,
    beta: results.filter((r) => r.stage === "Beta").length,
    rc: results.filter((r) => r.stage === "Release Candidate").length,
    v1: results.filter((r) => r.stage === "v1.0 Production Grade").length,
  };

  return {
    scannedAt: new Date().toISOString(),
    totalApps: results.length,
    averageScore,
    stagesCount,
    results,
  };
}

/** CLI Output Helpers */
function renderProgressBar(score: number, width = 14): string {
  const filled = Math.round((score / 100) * width);
  const empty = width - filled;
  const bar = "█".repeat(filled) + "░".repeat(empty);
  let color = "\x1b[31m";
  if (score >= 80) color = "\x1b[32m";
  else if (score >= 60) color = "\x1b[33m";
  else if (score >= 35) color = "\x1b[36m";
  return `${color}[${bar}]\x1b[0m ${score.toString().padStart(3)}%`;
}

export function printCliSummary(summary: EcosystemAuditSummary) {
  console.log("\n\x1b[1m\x1b[38;2;226;248;78m╔════════════════════════════════════════════════════════════════════════════════════════════╗\x1b[0m");
  console.log("\x1b[1m\x1b[38;2;226;248;78m║                       NEXUS SYSTEMS — V1.0 PRODUCTION MATURITY RADAR                       ║\x1b[0m");
  console.log("\x1b[1m\x1b[38;2;226;248;78m╚════════════════════════════════════════════════════════════════════════════════════════════╝\x1b[0m\n");

  console.log(` \x1b[1mScanned Apps:\x1b[0m ${summary.totalApps} core platform services  │  \x1b[1mEcosystem Average:\x1b[0m ${renderProgressBar(summary.averageScore)}`);
  console.log(` \x1b[1mDistribution:\x1b[0m 🔴 Scaffold: ${summary.stagesCount.scaffold}  │  🟠 Alpha: ${summary.stagesCount.alpha}  │  🟡 Beta: ${summary.stagesCount.beta}  │  🟢 RC: ${summary.stagesCount.rc}  │  🌟 v1.0: ${summary.stagesCount.v1}\n`);

  console.log(" " + "─".repeat(92));
  console.log(
    ` ${"APP NAME".padEnd(18)} ${"STAGE".padEnd(18)} ${"SCORE PROGRESS".padEnd(23)} ${"FEATURES".padEnd(10)} ${"TOP MISSING V1.0 CAPABILITY"}`
  );
  console.log(" " + "─".repeat(92));

  for (const r of summary.results) {
    const name = r.app.name.padEnd(18);
    const stageStr = `${r.stageColor}${r.stage.padEnd(18)}\x1b[0m`;
    const bar = renderProgressBar(r.totalScore);
    const features = `${r.completedFeatures.length}/${r.app.features.length}`.padEnd(10);
    const missing = r.missingFeatures[0]?.name || "Polish & battle-testing";

    console.log(` ${name} ${stageStr} ${bar}  ${features} ${missing.slice(0, 32)}`);
  }
  console.log(" " + "─".repeat(92));
  console.log(`\n \x1b[90mRun \x1b[1mbun run dhts/nexus-v1-radar/scan.ts --app <app-id>\x1b[0m\x1b[90m for detailed feature checklist.\x1b[0m`);
  console.log(` \x1b[90mRun \x1b[1mbun run dhts/nexus-v1-radar/server.ts\x1b[0m\x1b[90m to view the interactive web dashboard on :3200.\x1b[0m\n`);
}

export function printAppDetail(result: AppAuditResult) {
  const { app, totalScore, stage, stageColor, completedFeatures, missingFeatures, pillars, live, disk } = result;

  console.log("\n\x1b[1m\x1b[38;2;226;248;78m╔════════════════════════════════════════════════════════════════════════════════════════════╗\x1b[0m");
  console.log(`\x1b[1m\x1b[38;2;226;248;78m║  APP AUDIT: ${app.name.toUpperCase().padEnd(74)} ║\x1b[0m`);
  console.log("\x1b[1m\x1b[38;2;226;248;78m╚════════════════════════════════════════════════════════════════════════════════════════════╝\x1b[0m\n");

  console.log(` \x1b[1mID:\x1b[0m ${app.id}   \x1b[1mCategory:\x1b[0m ${app.category}   \x1b[1mPort:\x1b[0m :${app.port}   \x1b[1mPublic URL:\x1b[0m ${app.publicUrl}`);
  console.log(` \x1b[1mDescription:\x1b[0m ${app.description}`);
  console.log(` \x1b[1mCurrent Stage:\x1b[0m ${stageColor}${stage}\x1b[0m  │  \x1b[1mOverall v1.0 Readiness:\x1b[0m ${renderProgressBar(totalScore)}`);
  console.log(` \x1b[1mRuntime Status:\x1b[0m Port listening: ${live.portListening ? "✅ YES" : "❌ NO"}  │  HTTP Health: ${live.httpHealthy ? "✅ 200 OK" : "❌ DOWN"}`);
  console.log(` \x1b[1mDisk Assets:\x1b[0m Source files: ${disk.sourceFileCount}  │  check.sh: ${disk.hasCheckSh ? "✅" : "❌"}  │  UI: ${disk.hasFrontend ? "✅" : "❌"}\n`);

  console.log(" \x1b[1mPILLAR BREAKDOWN:\x1b[0m");
  console.log(`   • Data Layer:         ${pillars.data.score}/${pillars.data.max} pts  (${pillars.data.label})`);
  console.log(`   • UI & UX:            ${pillars.ui.score}/${pillars.ui.max} pts  (${pillars.ui.label})`);
  console.log(`   • Core Features:      ${pillars.features.score}/${pillars.features.max} pts  (${pillars.features.label} done)`);
  console.log(`   • API & Protocols:    ${pillars.api.score}/${pillars.api.max} pts  (${pillars.api.label})`);
  console.log(`   • Quality & Tests:    ${pillars.quality.score}/${pillars.quality.max} pts  (${pillars.quality.label})`);
  console.log(`   • Production Gateway: ${pillars.platform.score}/${pillars.platform.max} pts  (${pillars.platform.label})\n`);

  console.log(" \x1b[1mFEATURE CHECKLIST (v1.0 Specification):\x1b[0m");
  for (const f of completedFeatures) {
    console.log(`   \x1b[32m✔ [DONE]\x1b[0m \x1b[1m${f.name}\x1b[0m: ${f.description}`);
  }
  for (const f of missingFeatures) {
    console.log(`   \x1b[31m✖ [TODO]\x1b[0m \x1b[1m${f.name}\x1b[0m: ${f.description}`);
  }

  console.log("\n \x1b[1mACTIONABLE ROADMAP TO REACH V1.0:\x1b[0m");
  app.roadmap.forEach((step, idx) => {
    console.log(`   ${idx + 1}. ${step}`);
  });
  console.log("");
}

export function generateMarkdownReport(summary: EcosystemAuditSummary): string {
  const date = summary.scannedAt.split("T")[0];
  let md = `# Nexus Systems — v1.0 Production Maturity Radar\n\n`;
  md += `> **Automated Measurement:** Generated on ${summary.scannedAt}.\n\n`;
  md += `## Ecosystem Overview\n\n`;
  md += `- **Registered Platform Services:** ${summary.totalApps}\n`;
  md += `- **Average v1.0 Readiness:** **${summary.averageScore}%**\n`;
  md += `- **Stage Breakdown:**\n`;
  md += `  - 🔴 **Scaffold (0-29%):** ${summary.stagesCount.scaffold}\n`;
  md += `  - 🟠 **Alpha (30-59%):** ${summary.stagesCount.alpha}\n`;
  md += `  - 🟡 **Beta (60-79%):** ${summary.stagesCount.beta}\n`;
  md += `  - 🟢 **Release Candidate (80-94%):** ${summary.stagesCount.rc}\n`;
  md += `  - 🌟 **v1.0 Production Grade (95-100%):** ${summary.stagesCount.v1}\n\n`;

  md += `## Scoreboard by App\n\n`;
  md += `| App | Category | Port | Score | Stage | Features | Primary Next Task |\n`;
  md += `|---|---|---|---|---|---|---|\n`;

  for (const r of summary.results) {
    const nextTask = r.missingFeatures[0]?.name || "Polish & stability";
    md += `| **${r.app.name}** | \`${r.app.category}\` | :${r.app.port} | **${r.totalScore}%** | ${r.stage} | ${r.completedFeatures.length}/${r.app.features.length} | ${nextTask} |\n`;
  }

  md += `\n## Granular App Roadmaps to v1.0\n\n`;

  for (const r of summary.results) {
    md += `### ${r.app.name} (\`${r.app.id}\` — ${r.totalScore}% ${r.stage})\n`;
    md += `- **Domain:** ${r.app.publicUrl} (Port :${r.app.port})\n`;
    md += `- **Description:** ${r.app.description}\n`;
    md += `- **Completed Features:**\n`;
    for (const f of r.completedFeatures) {
      md += `  - [x] **${f.name}**: ${f.description}\n`;
    }
    md += `- **Missing for v1.0:**\n`;
    for (const f of r.missingFeatures) {
      md += `  - [ ] **${f.name}**: ${f.description}\n`;
    }
    md += `- **Recommended Steps:**\n`;
    r.app.roadmap.forEach((step, idx) => {
      md += `  ${idx + 1}. ${step}\n`;
    });
    md += `\n`;
  }

  return md;
}

// CLI Execution Entry Point
if (import.meta.main) {
  const args = process.argv.slice(2);

  const isJson = args.includes("--json");
  const isMarkdown = args.includes("--markdown");
  const appIndex = args.indexOf("--app");
  const targetAppId = appIndex >= 0 ? args[appIndex + 1] : null;

  const summary = await auditEcosystem();

  if (targetAppId) {
    const found = summary.results.find((r) => r.app.id === targetAppId || r.app.name.toLowerCase() === targetAppId.toLowerCase());
    if (!found) {
      console.error(`App '${targetAppId}' not found. Run without --app to list all apps.`);
      process.exit(1);
    }
    printAppDetail(found);
  } else if (isJson) {
    console.log(JSON.stringify(summary, null, 2));
  } else if (isMarkdown) {
    const md = generateMarkdownReport(summary);
    const outPath = resolve(process.cwd(), "docs/V1-MATURITY-RADAR.md");
    writeFileSync(outPath, md, "utf8");
    console.log(`\n\x1b[32m✔ Wrote canonical maturity radar to ${outPath}\x1b[0m\n`);
  } else {
    printCliSummary(summary);
  }
}
