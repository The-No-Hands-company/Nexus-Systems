import { createServer } from "./server";

const { close } = createServer();

process.on("SIGTERM", () => { close(); process.exit(0); });
process.on("SIGINT", () => { close(); process.exit(0); });
