/**
 * End to end: a real Nexus Cloud, Deploy's real publisher, and this proxy.
 *
 * Run by hand (it starts a Cloud of its own on a spare port, with its own empty
 * data directory, so it never touches the running one):
 *
 *   bun run deploy/production/tests/deploy-edge.e2e.ts
 *
 * It is not named *.test.ts on purpose: it starts a child process and binds
 * ports, so `bun test` should not pick it up.
 */
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { CloudPublisher, cloudNameTaken } from "../../../apps/Nexus-Deploy/src/lib/cloud.ts";

const CLOUD_PORT = 18788;
const APP_A = 20555;
const APP_B = 20556;

process.env.DOMAIN = "nexus.local";
process.env.CLOUD_URL = `http://127.0.0.1:${CLOUD_PORT}`;
process.env.NEXUS_CLOUD_API_KEY = "e2e-node-key";
process.env.CACHE_TTL_MS = "1";
process.env.GATE_SKIP_AUTH = "true";

const dataDir = mkdtempSync(join(tmpdir(), "deploy-edge-e2e-"));
const cloudProc = Bun.spawn(
  ["bun", "run", resolve(import.meta.dir, "../../../apps/Nexus-Cloud/src/server.ts")],
  {
    cwd: dataDir,
    env: {
      ...process.env,
      PORT: String(CLOUD_PORT),
      NEXUS_CLOUD_DOMAIN: "nexus.local",
      NEXUS_CLOUD_USER_APPS_KEY: "e2e-user-apps-key",
      NEXUS_CLOUD_API_KEY: "e2e-node-key",
    },
    stdout: "ignore",
    stderr: "ignore",
  },
);

const servers: Array<{ stop(force?: boolean): void }> = [];
const results: string[] = [];
const check = (name: string, ok: boolean, detail = "") => {
  results.push(`${ok ? "PASS" : "FAIL"} ${name}${detail ? ` — ${detail}` : ""}`);
};

async function waitForCloud(): Promise<void> {
  for (let i = 0; i < 50; i++) {
    try {
      if ((await fetch(`http://127.0.0.1:${CLOUD_PORT}/health`)).ok) return;
    } catch {
      // not up yet
    }
    await Bun.sleep(100);
  }
  throw new Error("the scratch Cloud did not start");
}

try {
  await waitForCloud();

  let seen: Record<string, string | null> = {};
  servers.push(
    Bun.serve({
      port: APP_A,
      hostname: "127.0.0.1",
      fetch(req) {
        seen = {
          cookie: req.headers.get("cookie"),
          tag: req.headers.get("x-nexus-client-tag"),
          ip: req.headers.get("cf-connecting-ip"),
        };
        return new Response("hello from my-app");
      },
    }),
    Bun.serve({ port: APP_B, hostname: "127.0.0.1", fetch: () => new Response("v2") }),
  );
  const hosting = Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch: () => new Response("No site found for x", { status: 404 }),
  });
  const control = Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch: () => new Response("deploy control plane"),
  });
  servers.push(hosting, control);
  process.env.HOSTING_SITE_UPSTREAM = `http://127.0.0.1:${hosting.port}`;
  process.env.DEPLOY_UPSTREAM = `http://127.0.0.1:${control.port}`;

  const { handleRequest, __setMaintenanceForTest } = await import("../proxy.ts");
  __setMaintenanceForTest([]);
  const cloud = { url: process.env.CLOUD_URL, key: "e2e-user-apps-key", timeoutMs: 3000 };
  const route = (port: number) => ({
    projectId: "p1",
    slug: "my-app",
    host: "my-app.nexus.local",
    hostPort: port,
  });

  check("the name is free before anything is published", (await cloudNameTaken(cloud, "my-app")) === false);

  const publisher = new CloudPublisher(cloud);
  await publisher.publish(route(APP_A));
  await Bun.sleep(20);

  const res = await handleRequest(
    new Request("http://my-app.nexus.local/", {
      headers: { cookie: "nexus_session=victim", "cf-connecting-ip": "203.0.113.9" },
    }),
  );
  check(
    "the app answers through the proxy",
    res.status === 200 && (await res.text()) === "hello from my-app",
    `status ${res.status}`,
  );
  check(
    "the app saw no cookie, no visitor tag and no address",
    seen.cookie === null && seen.tag === null && seen.ip === null,
    JSON.stringify(seen),
  );

  const link = await handleRequest(new Request("http://deploy.nexus.local/my-app"));
  check(
    "deploy.<domain>/<name> redirects to the app",
    link.status === 302 && link.headers.get("location") === "https://my-app.nexus.local/",
    `${link.status} ${link.headers.get("location")}`,
  );

  await publisher.publish(route(APP_B));
  await Bun.sleep(20);
  const moved = await handleRequest(new Request("http://my-app.nexus.local/"));
  check("publishing again moves the route to the new port", (await moved.text()) === "v2");

  const stolen = await fetch(`${cloud.url}/api/v1/user-apps/my-app`, {
    method: "PUT",
    headers: { authorization: "Bearer e2e-node-key", "content-type": "application/json" },
    body: JSON.stringify({ upstreamUrl: `http://127.0.0.1:${APP_A}` }),
  });
  check("the node API key cannot publish a member's app", stolen.status === 401, `${stolen.status}`);

  await publisher.unpublish("my-app");
  await Bun.sleep(20);
  const gone = await handleRequest(new Request("http://my-app.nexus.local/"));
  check("after unpublishing, the host falls through to Hosting", gone.status === 404, `${gone.status}`);
  const goneLink = await handleRequest(new Request("http://deploy.nexus.local/my-app"));
  check(
    "after unpublishing, the short link no longer redirects",
    goneLink.status === 200 && (await goneLink.text()) === "deploy control plane",
    `${goneLink.status}`,
  );
} finally {
  for (const server of servers) server.stop(true);
  cloudProc.kill();
  rmSync(dataDir, { recursive: true, force: true });
}

console.log(results.join("\n"));
process.exit(results.length === 0 || results.some((r) => r.startsWith("FAIL")) ? 1 : 0);
