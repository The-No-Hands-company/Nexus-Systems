import { createTestingServer } from "./server";
const PORT = Number(process.env["PORT"] ?? "3037");
const { server, close } = createTestingServer();
server.listen(PORT, () => {
  const a = server.address() as { port: number };
  process.stdout.write(
    JSON.stringify({
      level: "info",
      service: "nexus-testing",
      message: "listening",
      port: a.port,
    }) + "\n",
  );
});
async function shutdown(signal: string): Promise<void> {
  process.stdout.write(
    JSON.stringify({ level: "info", service: "nexus-testing", message: "shutting down", signal }) +
      "\n",
  );
  await close();
  process.exit(0);
}
process.on("SIGTERM", () => void shutdown("SIGTERM"));
process.on("SIGINT", () => void shutdown("SIGINT"));
