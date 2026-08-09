import { createSearchServer } from "./server";
const { close } = await createSearchServer();
process.on("SIGTERM", () => { close(); process.exit(0); });
process.on("SIGINT", () => { close(); process.exit(0); });
