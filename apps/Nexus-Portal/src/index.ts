import { createServer } from "./server";

const handle = await createServer();
const { close } = handle;

process.on("SIGTERM", () => {
  close();
  process.exit(0);
});
process.on("SIGINT", () => {
  close();
  process.exit(0);
});
