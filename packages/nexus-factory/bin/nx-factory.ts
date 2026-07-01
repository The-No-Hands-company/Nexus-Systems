#!/usr/bin/env bun
// ─── nx-factory — Nexus Software Factory CLI ───────────────────────────────
// Usage:
//   nx-factory create <app-name>         Scaffold a new Nexus app
//   nx-factory generate endpoint <name>  Generate a route handler from a schema
//   nx-factory check                     Run typecheck + lint + test

import { runCheck } from "../src/commands/check";
import { createApp } from "../src/commands/create";
import { generateEndpoint } from "../src/commands/generate";

function printUsage(): void {
  process.stderr.write(`nx-factory — Nexus Software Factory

Usage:
  nx-factory create <app-name> [--port <port>] [--description <desc>]
      Scaffold a new Nexus app in apps/<app-name>/
      Example: nx-factory create nexus-warehouse --port 3040

  nx-factory generate endpoint <name> [--app <app-name>] [--method <METHOD>] [--path <path>]
      Generate a route handler, validation, and test stubs
      Example: nx-factory generate endpoint create-alert --app nexus-monitor --method POST --path /api/v1/monitor/alerts

  nx-factory check [--app <app-name>]
      Run typecheck + lint + test (all apps if no --app specified)

  nx-factory --version
      Print version

`);
  process.exit(1);
}

async function main(): Promise<void> {
  const args = process.argv.slice(2);

  if (args.length === 0 || args[0] === "--help" || args[0] === "-h") {
    printUsage();
  }

  if (args[0] === "--version") {
    process.stdout.write("nx-factory 0.1.0\n");
    process.exit(0);
  }

  const command = args[0];
  const rest = args.slice(1);

  // Parse --key value flags from rest
  function getFlag(name: string): string | undefined {
    const idx = rest.indexOf(`--${name}`);
    if (idx >= 0 && idx < rest.length - 1 && !rest[idx + 1]?.startsWith("--")) {
      return rest[idx + 1];
    }
    return undefined;
  }

  switch (command) {
    case "create": {
      const appName = rest[0];
      if (!appName) {
        process.stderr.write("Error: <app-name> is required for 'create'\n");
        process.exit(1);
      }
      const port = getFlag("port");
      const description = getFlag("description");
      await createApp(appName, { port, description });
      break;
    }

    case "generate": {
      const subcommand = rest[0];
      if (!subcommand || subcommand.startsWith("--")) {
        process.stderr.write("Error: 'generate' requires a subcommand (e.g. 'endpoint')\n");
        process.exit(1);
      }
      if (subcommand === "endpoint") {
        const name = rest[1];
        if (!name || name.startsWith("--")) {
          process.stderr.write("Error: <name> is required for 'generate endpoint'\n");
          process.exit(1);
        }
        const app = getFlag("app");
        const method = getFlag("method");
        const path = getFlag("path");
        await generateEndpoint(name, { app, method, path });
      } else {
        process.stderr.write(`Error: unknown generate subcommand '${subcommand}'\n`);
        process.exit(1);
      }
      break;
    }

    case "check": {
      const app = getFlag("app");
      await runCheck(app);
      break;
    }

    default:
      process.stderr.write(`Error: unknown command '${command}'\n`);
      printUsage();
  }
}

main().catch((err) => {
  process.stderr.write(`Error: ${err instanceof Error ? err.message : String(err)}\n`);
  process.exit(1);
});
