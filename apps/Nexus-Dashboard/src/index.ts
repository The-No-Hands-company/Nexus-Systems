import { startServer } from "./server";

/**
 * Entrypoint. Kept separate from server.ts so the server stays importable by
 * tests, and so shutdown handling lives in one obvious place.
 *
 * deploy.sh stops services by signalling them, so honouring SIGTERM and SIGINT
 * is what makes `deploy.sh stop` a clean stop rather than a kill.
 */
const server = startServer();

for (const signal of ["SIGTERM", "SIGINT"] as const) {
  process.on(signal, () => {
    server.stop(true);
    process.exit(0);
  });
}
