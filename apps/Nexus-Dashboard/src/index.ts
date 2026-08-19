import { startCloudHeartbeat } from "./cloud";
import { startServer } from "./server";

/**
 * Entrypoint. Kept separate from server.ts so the server stays importable by
 * tests, and so shutdown handling lives in one obvious place.
 *
 * deploy.sh stops services by signalling them, so honouring SIGTERM and SIGINT
 * is what makes `deploy.sh stop` a clean stop rather than a kill.
 */
const server = startServer();

// Cloud marks a tool offline when it stops hearing from it, and Guardian
// refuses to expose an offline tool — so without this, app.<domain> never
// routes no matter how healthy the process actually is.
const stopHeartbeat = startCloudHeartbeat(
  process.env.NEXUS_DASHBOARD_UPSTREAM_URL || `http://127.0.0.1:${server.port}`,
);

for (const signal of ["SIGTERM", "SIGINT"] as const) {
  process.on(signal, () => {
    stopHeartbeat();
    server.stop(true);
    process.exit(0);
  });
}
