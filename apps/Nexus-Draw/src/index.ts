import { createServer } from "./server";

const { close } = await createServer();
console.log("[nexus-draw] Server ready");

process.on("SIGTERM", () => { close(); process.exit(0); });
process.on("SIGINT", () => { close(); process.exit(0); });

// Keep process alive
await new Promise(() => {});
