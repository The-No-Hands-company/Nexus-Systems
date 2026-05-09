import { createFilesServer } from "./server";
const PORT = Number(process.env["PORT"] ?? "3033");
const { server, close } = createFilesServer();
server.listen(PORT, () => {
  const a = server.address() as { port: number };
  process.stdout.write(
    JSON.stringify({ level: "info", service: "nexus-files", message: "listening", port: a.port }) +
      "\n",
  );
});
async function shutdown(signal: string): Promise<void> {
  process.stdout.write(
    JSON.stringify({ level: "info", service: "nexus-files", message: "shutting down", signal }) +
      "\n",
  );
  await close();
  process.exit(0);
}
process.on("SIGTERM", () => void shutdown("SIGTERM"));
process.on("SIGINT", () => void shutdown("SIGINT"));
