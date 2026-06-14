import { scaffold } from "./generate";

const args = process.argv.slice(2);

if (args.length < 2) {
  console.error("Usage: bun run ghost/src/index.ts scaffold <name> [--port PORT] [--description DESC]");
  process.exit(1);
}

const [command, appName, ...rest] = args as [string, string, ...string[]];

if (command !== "scaffold") {
  console.error(`Unknown command: ${command}`);
  console.error("Usage: bun run ghost/src/index.ts scaffold <name> [--port PORT] [--description DESC]");
  process.exit(1);
}

let port = 3080;
let description = "";

for (let i = 0; i < rest.length; i++) {
  if (rest[i] === "--port" && rest[i + 1]) {
    port = parseInt(rest[i + 1]!, 10) || 3080;
    i++;
  } else if (rest[i] === "--description" && rest[i + 1]) {
    description = rest[i + 1]!;
    i++;
  }
}

scaffold({ name: appName, port, description }).catch((err) => {
  console.error("Scaffold failed:", err);
  process.exit(1);
});
