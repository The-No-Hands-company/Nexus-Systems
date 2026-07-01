#!/usr/bin/env node
const VERSION = "2.0.0";
import { runScan, findFreePorts } from './scan.js';
import { printReport } from './report.js';
import { createServer } from './server.js';
import {
  applyPortReconfigurationPlanStub,
  createPortReconfigurationPlan,
  findPortConflicts,
  getPortToolCapabilities,
} from './port-management.js';

// ─────────────────────────────────────────────────────────────────────────────
// CLI argument parser
// ─────────────────────────────────────────────────────────────────────────────

interface CliOpts {
  command: 'scan' | 'serve' | 'free' | 'help' | 'version' | 'capabilities' | 'conflicts' | 'plan';
  probe: boolean;
  json: boolean;
  noColor: boolean;
  apiPort: number;
  freeFrom: number;
  freeTo: number;
  conflictPorts: number[];
  planFrom: number;
  planTo?: number;
  planRangeFrom: number;
  planRangeTo: number;
}

function parseArgs(argv: string[]): CliOpts {
  const args = argv.slice(2);

  const flag   = (f: string)     => args.includes(f);
  const argVal = (prefix: string) => args.find(a => a.startsWith(prefix))?.slice(prefix.length);

  let command: CliOpts['command'] = 'scan';
  if (flag('serve') || flag('--serve'))         command = 'serve';
  else if (flag('free') || flag('--free'))       command = 'free';
  else if (flag('capabilities') || flag('intel') || flag('--capabilities')) command = 'capabilities';
  else if (flag('conflicts') || flag('--conflicts')) command = 'conflicts';
  else if (flag('plan') || flag('reconfigure') || flag('--plan')) command = 'plan';
  else if (flag('help') || flag('--help') || flag('-h')) command = 'help';
  else if (flag('--version') || flag('-v')) command = 'version';

  const numericArgsAfter = (keys: string[]): number[] => {
    const idx = args.findIndex(a => keys.includes(a));
    if (idx < 0) return [];
    return args.slice(idx + 1).filter(a => /^\d+$/.test(a)).map(v => parseInt(v, 10));
  };

  // `free [from] [to]` — positional args after the keyword
  const freeArgs = numericArgsAfter(['free', '--free']);

  // `conflicts [port1] [port2] ...`
  const conflictPositional = numericArgsAfter(['conflicts', '--conflicts']);
  const conflictFromFlag = (argVal('--ports=') ?? '')
    .split(',')
    .map(s => parseInt(s.trim(), 10))
    .filter(n => Number.isFinite(n));

  // `plan <fromPort> [toPort]`
  const planArgs = numericArgsAfter(['plan', '--plan', 'reconfigure']);
  const parsedPlanFrom = parseInt(argVal('--from=') ?? '') || planArgs[0] || 3000;
  const parsedPlanToRaw = parseInt(argVal('--to=') ?? '') || planArgs[1] || 0;
  const parsedPlanTo = parsedPlanToRaw > 0 ? parsedPlanToRaw : undefined;

  return {
    command,
    probe:   flag('--probe') || flag('-p'),
    json:    flag('--json')  || flag('-j'),
    noColor: flag('--no-color') || !process.stdout.isTTY,
    apiPort: parseInt(argVal('--api-port=') ?? argVal('--port=') ?? '') || 2978,
    freeFrom: freeArgs[0] ?? 1024,
    freeTo:   freeArgs[1] ?? 9999,
    conflictPorts: (conflictPositional.length > 0 ? conflictPositional : conflictFromFlag)
      .filter(p => p >= 1 && p <= 65535),
    planFrom: parsedPlanFrom,
    planTo: parsedPlanTo,
    planRangeFrom: parseInt(argVal('--range-from=') ?? '') || 2000,
    planRangeTo: parseInt(argVal('--range-to=') ?? '') || 20000,
  };
}

// ─────────────────────────────────────────────────────────────────────────────
// Help text
// ─────────────────────────────────────────────────────────────────────────────

function printHelp(): void {
  console.log(`
  \x1b[1m\x1b[35m⬡ NEXUS PORTER\x1b[0m — Port Intelligence Tool

  \x1b[1mUsage:\x1b[0m
    npx tsx src/index.ts [command] [flags]

  \x1b[1mCommands:\x1b[0m
    (none)               Scan and print port table (default)
    serve                Start JSON API server (default port: 2978)
    free [from] [to]     List free ports in range (default: 1024–9999)
    capabilities          Show implemented vs stubbed port-intelligence features
    conflicts [ports...]  Check if selected ports are taken (or use --ports=3000,5432)
    plan <from> [to]      Build dry-run reconfiguration plan for a port move
    help                 Show this help

  \x1b[1mFlags:\x1b[0m
    --probe, -p          Probe HTTP endpoints — adds status code, title,
                         X-Frame-Options and iframe embeddability info
    --json, -j           Output raw JSON (scan command only)
    --no-color           Disable ANSI colour output
    --api-port=N         API server port (default: 2978)
    --ports=A,B,C        Comma-separated list for conflicts command
    --from=N             Source port for plan command
    --to=N               Target port for plan command
    --range-from=N       Candidate range start when auto-selecting plan target
    --range-to=N         Candidate range end when auto-selecting plan target

  \x1b[1mAPI Endpoints\x1b[0m (serve mode):
    GET /                Summary + metadata
    GET /ports           All listening ports (full detail)
    GET /ports/:n        Single port info
    GET /scan            Force fresh scan (bypasses 15s cache)
    GET /available       Free ports in range (?from=3000&to=4000)
    GET /capabilities    Tool feature matrix (implemented + stubs)
    GET /conflicts       Check selected ports (?ports=3000,5432)
    GET /reconfigure/plan  Build dry-run move plan (?from=3000&to=3100)
    GET /outbound        Outbound intelligence roadmap (stub)
    GET /health          Health check

    Query params:
      ?probe=1           Enable HTTP probing for that request
      ?force=1           Bypass cache

  \x1b[1mExamples:\x1b[0m
    npx tsx src/index.ts                  # Quick port table
    npx tsx src/index.ts --probe          # Table + HTTP probe info
    npx tsx src/index.ts serve            # JSON API on :2978
    npx tsx src/index.ts free 3000 4000   # Show free ports in range
    npx tsx src/index.ts capabilities      # Feature matrix
    npx tsx src/index.ts conflicts 3000 5432
    npx tsx src/index.ts plan 3000 3100
    npx tsx src/index.ts --json           # JSON output to stdout

  \x1b[1mAI / automation:\x1b[0m
    Start the server once, then query:
      curl http://localhost:2978/ports
      curl http://localhost:2978/ports/5432
      curl http://localhost:2978/available?from=3000&to=4000
`);
}

// ─────────────────────────────────────────────────────────────────────────────
// Entrypoint
// ─────────────────────────────────────────────────────────────────────────────

async function main(): Promise<void> {
  const opts = parseArgs(process.argv);

  switch (opts.command) {
    case 'help':
      printHelp();
      process.exit(0);

    case 'version':
      console.log(`⬡ Nexus Porter v${VERSION}`);
      process.exit(0);

    case 'serve': {
      const server = createServer(opts.apiPort);
      server.listen(opts.apiPort, '0.0.0.0', () => {
        const base = `http://localhost:${opts.apiPort}`;
        console.log('');
        console.log(`  \x1b[1m\x1b[35m⬡ Nexus Porter\x1b[0m  API ready on \x1b[36m${base}\x1b[0m`);
        console.log(`  \x1b[90m→\x1b[0m ${base}/ports`);
        console.log(`  \x1b[90m→\x1b[0m ${base}/ports/:n`);
        console.log(`  \x1b[90m→\x1b[0m ${base}/scan?probe=1`);
        console.log(`  \x1b[90m→\x1b[0m ${base}/available?from=3000&to=4000`);
        console.log('');
      });
      // Keep process alive
      process.on('SIGINT',  () => { server.close(); process.exit(0); });
      process.on('SIGTERM', () => { server.close(); process.exit(0); });
      break;
    }

    case 'free': {
      const { freeFrom, freeTo } = opts;
      console.log(`\nSearching for free ports in range [${freeFrom}–${freeTo}]...\n`);
      const free = await findFreePorts(freeFrom, freeTo);
      if (opts.json) {
        console.log(JSON.stringify({ from: freeFrom, to: freeTo, free, count: free.length }, null, 2));
      } else {
        if (free.length === 0) {
          console.log('  No free ports found in that range.');
        } else {
          console.log(`  \x1b[32m${free.length}\x1b[0m free ports:\n`);
          // Print in groups of 10
          for (let i = 0; i < free.length; i += 10) {
            console.log('  ' + free.slice(i, i + 10).join('  '));
          }
        }
      }
      console.log('');
      break;
    }

    case 'capabilities': {
      const capabilities = getPortToolCapabilities();
      if (opts.json) {
        console.log(JSON.stringify({ capabilities }, null, 2));
      } else {
        console.log('\nPort intelligence capabilities:\n');
        for (const cap of capabilities) {
          const badge = cap.status === 'implemented' ? '\x1b[32mimplemented\x1b[0m' : '\x1b[33mstub\x1b[0m';
          console.log(`  - ${cap.name} [${badge}]`);
          console.log(`    ${cap.description}`);
          if (cap.nextSteps.length > 0) {
            console.log(`    next: ${cap.nextSteps.join(' | ')}`);
          }
        }
        console.log('');
      }
      break;
    }

    case 'conflicts': {
      const targets = opts.conflictPorts.length > 0 ? opts.conflictPorts : [opts.planFrom, opts.planTo].filter(Boolean) as number[];
      const report = await findPortConflicts(targets);
      if (opts.json) {
        console.log(JSON.stringify(report, null, 2));
      } else {
        const takenSummary = report.taken.length > 0
          ? report.taken.map(t => `${t.port} (${t.owner?.name ?? t.known?.name ?? 'unknown'})`).join(', ')
          : 'none';
        const freeSummary = report.free.length > 0 ? report.free.join(', ') : 'none';
        console.log('\nPort conflict report:\n');
        console.log(`  taken: ${takenSummary}`);
        console.log(`  free:  ${freeSummary}`);
        console.log('');
      }
      break;
    }

    case 'plan': {
      const plan = await createPortReconfigurationPlan({
        fromPort: opts.planFrom,
        toPort: opts.planTo,
        rangeFrom: opts.planRangeFrom,
        rangeTo: opts.planRangeTo,
      });

      if (opts.json) {
        console.log(JSON.stringify(plan, null, 2));
      } else {
        console.log('\nDry-run port reconfiguration plan:\n');
        console.log(`  from: ${plan.fromPort}`);
        console.log(`  suggested target: ${plan.candidatePort ?? 'none'}`);
        console.log(`  source listening: ${plan.current ? 'yes' : 'no'}`);
        if (plan.warnings.length > 0) {
          console.log(`  warnings: ${plan.warnings.join(' | ')}`);
        }
        console.log('  steps:');
        for (const step of plan.steps) console.log(`    - ${step}`);
        const apply = applyPortReconfigurationPlanStub(plan);
        console.log(`\n  apply mode: ${apply.mode} (${apply.message})\n`);
      }
      break;
    }

    case 'scan':
    default: {
      const result = await runScan({ probe: opts.probe, selfPort: opts.apiPort });
      if (opts.json) {
        console.log(JSON.stringify(result, null, 2));
      } else {
        printReport(result, opts.noColor);
      }
      break;
    }
  }
}

main().catch(err => {
  process.stderr.write(`\x1b[31mError:\x1b[0m ${(err as Error).message}\n`);
  process.exit(1);
});
