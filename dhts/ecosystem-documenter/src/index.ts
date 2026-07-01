import { parseArgs } from "node:util";
import path from "node:path";
import { readFileSync } from "node:fs";
import ecosystemConfig from "./config/ecosystem.config.json";
import { generateDocs } from "./core/generator";
import type { EcosystemConfig } from "./types";

const VERSION = "2.0.0";

function printHelp(): string {
  return `Ecosystem Documenter v${VERSION}

Usage:
  ecosystem-documenter generate [options]
  ecosystem-documenter --help
  ecosystem-documenter --version

Commands:
  generate    Generate documentation stubs

Options:
  --out-dir <path>    Output directory (default: generated-docs)
  --format <fmt>      Output format: json | md (default: md)
  --help, -h          Show this help
  --version, -v       Show version
`;
}

async function main(): Promise<void> {
  const args = process.argv.slice(2);

  // Handle help/version flags before parseArgs
  if (args.includes("--help") || args.includes("-h") || args.length === 0) {
    console.log(printHelp());
    return;
  }
  if (args.includes("--version") || args.includes("-v")) {
    console.log(`ecosystem-documenter v${VERSION}`);
    return;
  }

  const { values, positionals } = parseArgs({
    args,
    allowPositionals: true,
    options: {
      "out-dir": { type: "string", short: "o", default: "generated-docs" },
      format: { type: "string", short: "f", default: "md" },
    },
  });

  const command = positionals[0];
  if (!command) {
    console.log(printHelp());
    return;
  }

  if (command !== "generate") {
    console.error(`Unknown command: ${command}`);
    console.log(printHelp());
    process.exitCode = 1;
    return;
  }

  const config = ecosystemConfig as EcosystemConfig;
  const outputRoot = path.resolve(process.cwd(), values["out-dir"] as string);

  const result = await generateDocs(config, outputRoot);

  console.log(`Generated ${result.generatedFiles.length} documentation files in ${outputRoot}`);
}

void main().catch((error: unknown) => {
  console.error("Failed to generate documentation stubs.");
  console.error(error);
  process.exitCode = 1;
});
