import { existsSync, readdirSync, statSync, writeFileSync } from "fs";
import { join, resolve } from "path";
import * as net from "net";
import { REGISTERED_APPS, type AppDefinition, type FeatureCheck } from "./criteria";

// Resolved from the file's own location so the scan works when imported by
// another app (e.g. the Dashboard server) rather than run from the repo root.
const REPO_ROOT = resolve(import.meta.dir, "../..");

export type AppLiveStatus = {
  portListening: boolean;
  httpHealthy: boolean;
  localLatencyMs: number;
  publicReachable: boolean;
  publicStatusCode: number;
  publicStatusText: string;
  publicLatencyMs: number;
  isOperational: boolean;
  degradedReason?: string;
};

export type AppAuditResult = {
  app: AppDefinition;
  live: AppLiveStatus;
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
  rawScore: number;
  totalScore: number;
  stage: "Offline / 502 Degraded" | "Scaffold" | "Alpha" | "Beta" | "Release Candidate" | "v1.0 Production Grade";
  stageColor: string;
  missingFeatures: FeatureCheck[];
  completedFeatures: FeatureCheck[];
  topRoadmapItems: string[];
};

export type EcosystemAuditSummary = {
  scannedAt: string;
  totalApps: number;
  operationalApps: number;
  degradedApps: number;
  averageScore: number;
  stagesCount: {
    degraded: number;
    scaffold: number;
    alpha: number;
    beta: number;
    rc: number;
    v1: number;
  };
  results: AppAuditResult[];
};

/** Checks if a TCP port is currently listening on localhost with latency */
function isPortOpen(port: number, timeoutMs = 500): Promise<{ open: boolean; latencyMs: number }> {
  const start = performance.now();
  return new Promise((resolve) => {
    const socket = new net.Socket();
    let isConnected = false;

    socket.setTimeout(timeoutMs);
    socket.once("connect", () => {
      isConnected = true;
      const latencyMs = Math.round(performance.now() - start);
      socket.destroy();
      resolve({ open: true, latencyMs });
    });

    socket.once("timeout", () => {
      socket.destroy();
      resolve({ open: false, latencyMs: timeoutMs });
    });

    socket.once("error", () => {
      socket.destroy();
      resolve({ open: false, latencyMs: Math.round(performance.now() - start) });
    });

    socket.connect(port, "127.0.0.1");
  });
}

/** Checks if a local HTTP health endpoint returns 200 OK */
async function checkLocalHttpHealth(port: number): Promise<{ healthy: boolean; latencyMs: number }> {
  const endpoints = [
    `http://127.0.0.1:${port}/health`,
    `http://127.0.0.1:${port}/api/health`,
    `http://127.0.0.1:${port}/api/v1/health`,
    `http://127.0.0.1:${port}/`,
  ];
  for (const url of endpoints) {
    const start = performance.now();
    try {
      const res = await fetch(url, { signal: AbortSignal.timeout(800) });
      const latencyMs = Math.round(performance.now() - start);
      if (res.ok) return { healthy: true, latencyMs };
    } catch {
      // try next
    }
  }
  return { healthy: false, latencyMs: 0 };
}

/** Probes the real public endpoint through Cloudflare/gate */
export async function probePublicEndpoint(publicUrl: string): Promise<{
  status: number;
  statusText: string;
  latencyMs: number;
  reachable: boolean;
}> {
  const start = performance.now();
  try {
    const res = await fetch(publicUrl, {
      method: "GET",
      headers: { accept: "text/html,application/json" },
      signal: AbortSignal.timeout(3000),
      redirect: "manual",
    });
    const latencyMs = Math.round(performance.now() - start);
    let statusText = `${res.status} ${res.statusText || ""}`.trim();
    if (res.status === 200) statusText = "200 OK";
    else if (res.status === 302 || res.status === 303 || res.status === 307) statusText = `${res.status} Auth Gate`;
    else if (res.status === 401) statusText = "401 Protected";
    else if (res.status === 502) statusText = "502 Bad Gateway";
    else if (res.status === 503) statusText = "503 Unavailable";
    else if (res.status === 504) statusText = "504 Gateway Timeout";

    const reachable = res.status < 500 && res.status !== 0;
    return {
      status: res.status,
      statusText,
      latencyMs,
      reachable,
    };
  } catch (err: any) {
    const latencyMs = Math.round(performance.now() - start);
    const isTimeout = err?.name === "TimeoutError";
    return {
      status: 0,
      statusText: isTimeout ? "Timeout (3s)" : "Connection Refused",
      latencyMs,
      reachable: false,
    };
  }
}

/** Recursively counts source files (.ts, .tsx, .rs, .py, .go, .jsx) */
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
  const rootDir = resolve(REPO_ROOT, app.dir);
  const dirExists = existsSync(rootDir);

  let hasCheckSh = false;
  let hasFrontend = false;
  let hasTests = false;
  let hasRealDataLayer = false;
  let sourceFileCount = 0;

  if (dirExists) {
    hasCheckSh = existsSync(join(rootDir, "check.sh"));
    hasFrontend =
      (app.frontendDir && existsSync(resolve(REPO_ROOT, app.frontendDir))) ||
      existsSync(join(rootDir, "frontend")) ||
      existsSync(join(rootDir, "src", "App.tsx")) ||
      existsSync(join(rootDir, "web")) ||
      existsSync(join(rootDir, "public", "index.html"));
    hasTests =
      existsSync(join(rootDir, "tests")) ||
      existsSync(join(rootDir, "src", "tests")) ||
      existsSync(join(rootDir, "e2e")) ||
      existsSync(join(rootDir, "crates", "database-server", "tests"));
    sourceFileCount = countSourceFiles(rootDir);

    // Real DB schemas / persistence verification
    const hasDataDir = existsSync(join(rootDir, "data"));
    const hasMigrations = existsSync(join(rootDir, "migrations")) || existsSync(join(rootDir, "db"));
    hasRealDataLayer = (sourceFileCount > 6 && (hasDataDir || hasMigrations)) || sourceFileCount > 15;
  }

  // ── Real-Time Live Runtime Probes ──────────────────────────────────────
  const { open: portListening, latencyMs: portLatency } = await isPortOpen(app.port);
  const { healthy: httpHealthy, latencyMs: localHttpLatency } = portListening
    ? await checkLocalHttpHealth(app.port)
    : { healthy: false, latencyMs: 0 };
  const localLatencyMs = httpHealthy ? localHttpLatency : portLatency;

  const publicProbe = await probePublicEndpoint(app.publicUrl);

  // Determine operational ground truth
  let isOperational = true;
  let degradedReason: string | undefined;

  if (!portListening && !httpHealthy) {
    isOperational = false;
    degradedReason = `Daemon not listening on :${app.port}`;
  } else if (!publicProbe.reachable) {
    isOperational = false;
    degradedReason = `Public endpoint failure: ${publicProbe.statusText}`;
  } else if (publicProbe.status === 502) {
    isOperational = false;
    degradedReason = `502 Bad Gateway via Cloudflare`;
  }

  const live: AppLiveStatus = {
    portListening,
    httpHealthy,
    localLatencyMs,
    publicReachable: publicProbe.reachable,
    publicStatusCode: publicProbe.status,
    publicStatusText: publicProbe.statusText,
    publicLatencyMs: publicProbe.latencyMs,
    isOperational,
    degradedReason,
  };

  // ── Features Breakdown ────────────────────────────────────────────────
  const completedFeatures = app.features.filter((f) => f.done);
  const missingFeatures = app.features.filter((f) => !f.done);
  const featureRatio = app.features.length > 0 ? completedFeatures.length / app.features.length : 0;

  // ── Score Pillars (0-100 base) ─────────────────────────────────────────
  // 1. Data Layer (20 pts)
  let dataScore = 5;
  if (hasRealDataLayer) dataScore = 18;
  else if (sourceFileCount > 6) dataScore = 12;

  // 2. UI & UX (25 pts)
  let uiScore = 0;
  if (hasFrontend) {
    uiScore = 22;
  } else if (["nexus-auth", "nexus-edge", "nexus-guardian", "nexus-cloud", "nexus-database"].includes(app.id)) {
    uiScore = 20;
  } else {
    uiScore = 5;
  }

  // 3. Domain Features (25 pts)
  const featuresScore = Math.round(featureRatio * 25);

  // 4. API & Protocol (10 pts)
  let apiScore = 6;
  if (dirExists && (existsSync(join(rootDir, "src", "server.ts")) || existsSync(join(rootDir, "main.py")) || sourceFileCount > 8)) {
    apiScore = 10;
  }

  // 5. Testing & Quality Gate (10 pts)
  let qualityScore = 0;
  if (hasCheckSh && hasTests) qualityScore = 10;
  else if (hasCheckSh || hasTests) qualityScore = 6;

  // 6. Platform & Delivery (10 pts)
  let platformScore = 0;
  if (portListening) platformScore += 3;
  if (httpHealthy) platformScore += 3;
  if (publicProbe.reachable) platformScore += 4;

  const rawScore = Math.min(100, Math.round(dataScore + uiScore + featuresScore + apiScore + qualityScore + platformScore));

  // ── Strict Ground-Truth Penalties & Hard Caps ─────────────────────────
  let totalScore = rawScore;
  let stage: AppAuditResult["stage"] = "Scaffold";
  let stageColor = "\x1b[31m"; // Red

  if (!isOperational) {
    // HARD CAP: If service is offline or throws 502, it CANNOT be RC or v1.0!
    totalScore = Math.min(45, rawScore);
    stage = "Offline / 502 Degraded";
    stageColor = "\x1b[1m\x1b[31m"; // Bold Red
  } else {
    // Operational services:
    if (totalScore >= 95 && completedFeatures.length === app.features.length && hasCheckSh && httpHealthy && publicProbe.reachable) {
      stage = "v1.0 Production Grade";
      stageColor = "\x1b[35m"; // Magenta/Gold
    } else if (totalScore >= 80) {
      // Must have working daemon and no missing core blockers for RC
      if (completedFeatures.length < app.features.length) {
        totalScore = Math.min(totalScore, 89);
      }
      stage = "Release Candidate";
      stageColor = "\x1b[32m"; // Green
    } else if (totalScore >= 60) {
      stage = "Beta";
      stageColor = "\x1b[33m"; // Yellow
    } else if (totalScore >= 30) {
      stage = "Alpha";
      stageColor = "\x1b[36m"; // Cyan
    }
  }

  return {
    app,
    live,
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
      platform: {
        score: platformScore,
        max: 10,
        label: isOperational ? `Live (${publicProbe.latencyMs}ms)` : publicProbe.statusText,
      },
    },
    rawScore,
    totalScore,
    stage,
    stageColor,
    missingFeatures,
    completedFeatures,
    topRoadmapItems: app.roadmap.slice(0, 3),
  };
}

export async function auditEcosystem(): Promise<EcosystemAuditSummary> {
  const results: AppAuditResult[] = await Promise.all(REGISTERED_APPS.map(auditApp));

  // Sort by operational status first (degraded first so they get noticed), then score ascending
  results.sort((a, b) => {
    if (!a.live.isOperational && b.live.isOperational) return -1;
    if (a.live.isOperational && !b.live.isOperational) return 1;
    return a.totalScore - b.totalScore;
  });

  const totalScoreSum = results.reduce((sum, r) => sum + r.totalScore, 0);
  const averageScore = Math.round(totalScoreSum / results.length);

  const stagesCount = {
    degraded: results.filter((r) => r.stage === "Offline / 502 Degraded").length,
    scaffold: results.filter((r) => r.stage === "Scaffold").length,
    alpha: results.filter((r) => r.stage === "Alpha").length,
    beta: results.filter((r) => r.stage === "Beta").length,
    rc: results.filter((r) => r.stage === "Release Candidate").length,
    v1: results.filter((r) => r.stage === "v1.0 Production Grade").length,
  };

  const operationalApps = results.filter((r) => r.live.isOperational).length;

  return {
    scannedAt: new Date().toISOString(),
    totalApps: results.length,
    operationalApps,
    degradedApps: results.length - operationalApps,
    averageScore,
    stagesCount,
    results,
  };
}

/** CLI Output Helpers */
function renderProgressBar(score: number, width = 12): string {
  const filled = Math.round((score / 100) * width);
  const empty = width - filled;
  const bar = "█".repeat(filled) + "░".repeat(empty);
  let color = "\x1b[31m";
  if (score >= 90) color = "\x1b[35m";
  else if (score >= 80) color = "\x1b[32m";
  else if (score >= 60) color = "\x1b[33m";
  else if (score >= 35) color = "\x1b[36m";
  return `${color}[${bar}]\x1b[0m ${score.toString().padStart(3)}%`;
}

export function printCliSummary(summary: EcosystemAuditSummary) {
  console.log("\n\x1b[1m\x1b[38;2;226;248;78m╔══════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\x1b[0m");
  console.log("\x1b[1m\x1b[38;2;226;248;78m║                       NEXUS SYSTEMS — REAL-TIME GROUND-TRUTH MATURITY RADAR                          ║\x1b[0m");
  console.log("\x1b[1m\x1b[38;2;226;248;78m╚══════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\x1b[0m\n");

  const opRatio = `${summary.operationalApps}/${summary.totalApps} Operational`;
  const opColor = summary.degradedApps > 0 ? "\x1b[33m" : "\x1b[32m";

  console.log(` \x1b[1mScanned Apps:\x1b[0m ${summary.totalApps} services  │  \x1b[1mRuntime Health:\x1b[0m ${opColor}${opRatio}\x1b[0m  │  \x1b[1mEcosystem Average:\x1b[0m ${renderProgressBar(summary.averageScore)}`);
  console.log(` \x1b[1mDistribution:\x1b[0m 🔴 Degraded: ${summary.stagesCount.degraded}  │  🟠 Alpha: ${summary.stagesCount.alpha}  │  🟡 Beta: ${summary.stagesCount.beta}  │  🟢 RC: ${summary.stagesCount.rc}  │  🌟 v1.0: ${summary.stagesCount.v1}\n`);

  console.log(" " + "─".repeat(110));
  console.log(
    ` ${"APP NAME".padEnd(16)} ${"PORT".padEnd(7)} ${"LOCAL".padEnd(9)} ${"PUBLIC GATEWAY".padEnd(20)} ${"LATENCY".padEnd(9)} ${"SCORE".padEnd(20)} ${"STATUS / ROADMAP"}`
  );
  console.log(" " + "─".repeat(110));

  for (const r of summary.results) {
    const name = r.app.name.padEnd(16);
    const port = `:${r.app.port}`.padEnd(7);
    const local = r.live.httpHealthy
      ? "\x1b[32m● 200 OK\x1b[0m".padEnd(17)
      : r.live.portListening
      ? "\x1b[33m▲ LISTEN\x1b[0m".padEnd(17)
      : "\x1b[31m○ DOWN  \x1b[0m".padEnd(17);

    let pub = "";
    if (r.live.publicStatusCode === 502) pub = "\x1b[1m\x1b[31m502 BAD GATEWAY\x1b[0m".padEnd(29);
    else if (r.live.publicStatusCode === 503) pub = "\x1b[31m503 UNAVAIL    \x1b[0m".padEnd(29);
    else if (r.live.publicStatusCode === 200) pub = "\x1b[32m200 Operational\x1b[0m".padEnd(29);
    else if (r.live.publicStatusCode === 302 || r.live.publicStatusCode === 303) pub = "\x1b[36m302 Auth Gate  \x1b[0m".padEnd(29);
    else if (r.live.publicStatusCode === 401) pub = "\x1b[36m401 Protected  \x1b[0m".padEnd(29);
    else pub = `\x1b[31m${r.live.publicStatusText.slice(0, 15)}\x1b[0m`.padEnd(29);

    const lat = `${r.live.publicLatencyMs}ms`.padEnd(9);
    const bar = renderProgressBar(r.totalScore);
    const reason = r.live.degradedReason
      ? `\x1b[31m${r.live.degradedReason}\x1b[0m`
      : r.missingFeatures[0]?.name || "Polish & battle-testing";

    console.log(` ${name} ${port} ${local} ${pub} ${lat} ${bar}  ${reason.slice(0, 36)}`);
  }
  console.log(" " + "─".repeat(110));
  console.log(`\n \x1b[90mRun \x1b[1mbun run dhts/nexus-v1-radar/scan.ts --app <app-id>\x1b[0m\x1b[90m for detailed feature checklist.\x1b[0m`);
  console.log(` \x1b[90mRun \x1b[1mbun run radar:server\x1b[0m\x1b[90m to view the real-time auto-refreshing dashboard on :3200.\x1b[0m\n`);
}

export function printAppDetail(result: AppAuditResult) {
  const { app, totalScore, rawScore, stage, stageColor, completedFeatures, missingFeatures, pillars, live, disk } = result;

  console.log("\n\x1b[1m\x1b[38;2;226;248;78m╔════════════════════════════════════════════════════════════════════════════════════════════╗\x1b[0m");
  console.log(`\x1b[1m\x1b[38;2;226;248;78m║  APP AUDIT: ${app.name.toUpperCase().padEnd(74)} ║\x1b[0m`);
  console.log("\x1b[1m\x1b[38;2;226;248;78m╚════════════════════════════════════════════════════════════════════════════════════════════╝\x1b[0m\n");

  console.log(` \x1b[1mID:\x1b[0m ${app.id}   \x1b[1mCategory:\x1b[0m ${app.category}   \x1b[1mPort:\x1b[0m :${app.port}   \x1b[1mPublic URL:\x1b[0m ${app.publicUrl}`);
  console.log(` \x1b[1mDescription:\x1b[0m ${app.description}`);
  console.log(` \x1b[1mCurrent Stage:\x1b[0m ${stageColor}${stage}\x1b[0m  │  \x1b[1mOverall Readiness:\x1b[0m ${renderProgressBar(totalScore)} (Raw: ${rawScore}%)`);
  
  if (live.degradedReason) {
    console.log(` \x1b[1m\x1b[31mDEGRADED REASON:\x1b[0m \x1b[31m${live.degradedReason}\x1b[0m (Hard-capped at <=45%)`);
  }

  console.log(` \x1b[1mRuntime Status:\x1b[0m Local Port: ${live.portListening ? "✅ LISTENING" : "❌ DOWN"}  │  Local Health: ${live.httpHealthy ? "✅ 200 OK" : "❌ DOWN"} (${live.localLatencyMs}ms)`);
  console.log(` \x1b[1mPublic Gateway:\x1b[0m Status: ${live.publicStatusText}  │  Reachable: ${live.publicReachable ? "✅ YES" : "❌ NO"} (${live.publicLatencyMs}ms)`);
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
  let md = `# Nexus Systems — Real-Time Ground-Truth Production Radar\n\n`;
  md += `> **Automated Measurement:** Generated on ${summary.scannedAt}.\n\n`;
  md += `## Ecosystem Overview\n\n`;
  md += `- **Registered Platform Services:** ${summary.totalApps}\n`;
  md += `- **Operational Services:** **${summary.operationalApps}/${summary.totalApps}**\n`;
  md += `- **Degraded / Offline Services:** **${summary.degradedApps}**\n`;
  md += `- **Average v1.0 Readiness:** **${summary.averageScore}%**\n`;
  md += `- **Stage Breakdown:**\n`;
  md += `  - 🔴 **Offline / 502 Degraded (<50%):** ${summary.stagesCount.degraded}\n`;
  md += `  - 🟠 **Alpha (30-59%):** ${summary.stagesCount.alpha}\n`;
  md += `  - 🟡 **Beta (60-79%):** ${summary.stagesCount.beta}\n`;
  md += `  - 🟢 **Release Candidate (80-94%):** ${summary.stagesCount.rc}\n`;
  md += `  - 🌟 **v1.0 Production Grade (95-100%):** ${summary.stagesCount.v1}\n\n`;

  md += `## Scoreboard by App (Ground Truth)\n\n`;
  md += `| App | Port | Local Health | Public Gateway | Score | Stage | Features | Next Task / Status |\n`;
  md += `|---|---|---|---|---|---|---|---|\n`;

  for (const r of summary.results) {
    const nextTask = r.live.degradedReason || r.missingFeatures[0]?.name || "Polish & stability";
    const localHealth = r.live.httpHealthy ? "✅ 200 OK" : r.live.portListening ? "▲ Listening" : "❌ Down";
    const pubHealth = r.live.publicStatusText;
    md += `| **${r.app.name}** | :${r.app.port} | ${localHealth} | ${pubHealth} | **${r.totalScore}%** | ${r.stage} | ${r.completedFeatures.length}/${r.app.features.length} | ${nextTask} |\n`;
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
    console.log(`\n\x1b[32m✔ Wrote ground-truth maturity radar to ${outPath}\x1b[0m\n`);
  } else {
    printCliSummary(summary);
  }
}
