import { createServer } from "./server";

const { close } = await createServer();

let shutdown: Promise<void> | null = null;
for (const signal of ["SIGTERM", "SIGINT"] as const) {
  process.on(signal, () => {
    shutdown ??= close()
      .then(() => { process.exitCode = 0; })
      .catch((error) => {
        console.error(`[nexus-terminal] shutdown failed: ${(error as Error).message}`);
        process.exitCode = 1;
      });
  });
}
