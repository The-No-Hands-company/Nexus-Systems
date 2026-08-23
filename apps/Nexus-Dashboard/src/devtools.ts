import { spawn } from "node:child_process";
import { existsSync, readdirSync } from "node:fs";
import { join } from "node:path";

/**
 * The development helper tools (dhts/) wired into the Operator panel.
 *
 * Two halves:
 *
 * 1. A curated registry describing each tool: what it does, the command an
 *    operator would run by hand, and — where a bounded, useful invocation
 *    exists — a whitelisted `run` command this endpoint may execute.
 * 2. A directory scan that surfaces anything else sitting in dhts/ without
 *    registry metadata, so a new tool is never invisible just because nobody
 *    described it yet.
 *
 * Run commands come from THIS table and never from the request body. The only
 * caller input is which registry entry to run; everything else — argv, cwd,
 * timeout, output cap — is fixed here. That keeps this a founder-only
 * convenience rather than a generic command executor.
 */

export type DevTool = {
  id: string;
  /** Directory name under dhts/. */
  dir: string;
  name: string;
  description: string;
  /** The command an operator runs by hand, shown for copy/paste. */
  command: string;
  /** Present when this endpoint may run a bounded invocation of the tool. */
  runnable?: { cmd: string; args: string[] };
};

const DHTS_ROOT =
  process.env.NEXUS_DHTS_ROOT || join(import.meta.dir, "..", "..", "..", "dhts");

const REGISTRY: DevTool[] = [
  {
    id: "graph",
    dir: "ecosystem-graph",
    name: "Ecosystem Graph",
    description:
      "Extracts the app/dependency/connection graph from the monorepo into graph.json (schema v1). Input to the visualizer.",
    command: "python3 dhts/ecosystem-graph/extract_graph.py",
    runnable: { cmd: "python3", args: ["extract_graph.py"] },
  },
  {
    id: "visualizer",
    dir: "ecosystem-visualizer",
    name: "Ecosystem Visualizer",
    description:
      "Builds the topology/dependency visualizer from the extracted graph. Run after the graph extractor.",
    command: "cd dhts/ecosystem-visualizer && python3 build.py",
    runnable: { cmd: "python3", args: ["build.py"] },
  },
  {
    id: "documenter",
    dir: "ecosystem-documenter",
    name: "Ecosystem Documenter",
    description:
      "Auto-generates docs from code into generated-docs/. Bun workspace tool.",
    command: "cd dhts/ecosystem-documenter && bun run generate",
    runnable: { cmd: "bun", args: ["run", "generate"] },
  },
  {
    id: "porter",
    dir: "ecosystem-porter",
    name: "Ecosystem Porter",
    description:
      "Cross-app portability scan: how much of each app could move between hosts.",
    command: "cd dhts/ecosystem-porter && bun run scan",
    runnable: { cmd: "bun", args: ["run", "scan"] },
  },
  {
    id: "testsuit",
    dir: "ecosystem-internal-testsuit",
    name: "Internal Testsuite",
    description:
      "Ecosystem-level integration audit across services (bun run audit). Can take minutes.",
    command: "cd dhts/ecosystem-internal-testsuit && bun run audit",
    runnable: { cmd: "bun", args: ["run", "audit"] },
  },
  {
    id: "stack-control",
    dir: "ecosystem-stack-control-and-updater",
    name: "Stack Control & Updater",
    description:
      "Version and environment management across the ecosystem. See its README for subcommands.",
    command: "./dhts/ecosystem-stack-control-and-updater/run.sh",
  },
  {
    id: "autopilot",
    dir: "ecosystem-autopilot",
    name: "Ecosystem Autopilot",
    description:
      "Orchestration and automation driver (backlog, decisions, company state). Long-running; invoke by hand.",
    command: "python3 dhts/ecosystem-autopilot/autopilot.py",
  },
  {
    id: "editor-debugger",
    dir: "ecosystem-editor-debugger",
    name: "Editor Debugger",
    description: "Editor debugging helper CLI.",
    command: "python3 dhts/ecosystem-editor-debugger/editor_debugger.py --help",
  },
  {
    id: "vision-board",
    dir: "ecosystem-vision-board",
    name: "Vision Board",
    description:
      "Unreal-Blueprint-style interactive planning board. Serves a UI on a port; long-running, invoke by hand.",
    command: "python3 dhts/ecosystem-vision-board/server.py",
  },
  {
    id: "cpp-toolkit",
    dir: "ecosystem-cpp-utility-toolkit",
    name: "C++ Utility Toolkit",
    description:
      "Comprehensive C++ debugging/profiling toolkit (525 headers, 516 sources, 67 categories). Built with CMake; not runnable from here.",
    command: "cmake -S dhts/ecosystem-cpp-utility-toolkit -B build/dht && cmake --build build/dht",
  },
];

/** Directories present on disk but missing from REGISTRY above. */
function unregisteredDirs(): DevTool[] {
  if (!existsSync(DHTS_ROOT)) return [];
  const known = new Set(REGISTRY.map((t) => t.dir));
  const skip = new Set(["GameDevelopmentToolset", "nexus-pay"]); // unrelated projects living in dhts for now
  const out: DevTool[] = [];
  for (const entry of readdirSync(DHTS_ROOT, { withFileTypes: true })) {
    if (!entry.isDirectory() || known.has(entry.name) || skip.has(entry.name)) continue;
    out.push({
      id: entry.name,
      dir: entry.name,
      name: entry.name,
      description:
        "Unregistered helper discovered in dhts/ — describe it in src/devtools.ts REGISTRY to give it a real entry or make it runnable.",
      command: `dhts/${entry.name}/`,
    });
  }
  return out.sort((a, b) => a.id.localeCompare(b.id));
}

export function listDevTools(): { root: string; exists: boolean; tools: DevTool[] } {
  return {
    root: DHTS_ROOT,
    exists: existsSync(DHTS_ROOT),
    tools: [...REGISTRY, ...unregisteredDirs()],
  };
}

const RUN_TIMEOUT_MS = 150_000;
const OUTPUT_CAP = 200_000;

/** Runs the whitelisted invocation for a registry entry. Never client-supplied argv. */
export function runDevTool(
  id: string,
): Promise<{ ok: boolean; exitCode: number | null; output: string; error?: string }> {
  const tool = REGISTRY.find((t) => t.id === id);
  if (!tool?.runnable || !existsSync(join(DHTS_ROOT, tool.dir))) {
    return Promise.resolve({ ok: false, exitCode: null, output: "", error: "not_runnable" });
  }
  const cwd = join(DHTS_ROOT, tool.dir);
  return new Promise((resolve) => {
    let output = "";
    let capped = false;
    const child = spawn(tool.runnable!.cmd, tool.runnable!.args, { cwd, env: process.env });
    const timer = setTimeout(() => child.kill("SIGKILL"), RUN_TIMEOUT_MS);
    const collect = (chunk: Buffer | string) => {
      if (capped) return;
      output += chunk.toString();
      if (output.length > OUTPUT_CAP) {
        output = output.slice(0, OUTPUT_CAP) + "\n… output truncated";
        capped = true;
      }
    };
    child.stdout.on("data", collect);
    child.stderr.on("data", collect);
    child.on("error", (err) => {
      clearTimeout(timer);
      resolve({ ok: false, exitCode: null, output, error: err.message });
    });
    child.on("close", (code) => {
      clearTimeout(timer);
      resolve({ ok: code === 0, exitCode: code, output });
    });
  });
}
