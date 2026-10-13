#!/usr/bin/env bun
/**
 * Does every Dashboard page fit a phone, signed in?
 *
 *   bun scripts/check-dashboard-phone-width.ts [--shots <dir>]
 *
 * The Dashboard sits behind sign-in, so this starts a throwaway Nexus-Auth and
 * Dashboard server on free loopback ports, with every store, key and token in
 * a temporary directory and no cloud registration, signs in as a throwaway
 * founder, and runs scripts/check-phone-width.ts over each page with that
 * session. Other services (Cloud, Mail, Calendar) are pointed at a closed port,
 * so those pages are checked in their unavailable state.
 *
 * Needs the Dashboard frontend built (apps/Nexus-Dashboard/frontend/dist, or the
 * folder in NEXUS_DASHBOARD_WEB_ROOT — build there first to check a change
 * before it reaches the dist production serves).
 * Touches no production data and nothing outside its temporary directory.
 */

import { mkdtempSync, rmSync, existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { randomBytes } from "node:crypto";

const root = resolve(import.meta.dir, "..");
const PAGES = ["/", "/apps", "/account", "/admin", "/cloud", "/cloud/tools", "/cloud/federation",
  "/cloud/identity", "/cloud/api", "/mail", "/mail/compose", "/terminal", "/chat"];

function freePort(): number {
  const probe = Bun.serve({ port: 0, hostname: "127.0.0.1", fetch: () => new Response() });
  const port = probe.port;
  probe.stop(true);
  return port;
}

async function waitFor(url: string, label: string): Promise<void> {
  for (let i = 0; i < 100; i++) {
    if (await fetch(url).then((r) => r.ok, () => false)) return;
    await Bun.sleep(200);
  }
  throw new Error(`${label} did not start`);
}

const shotsIndex = process.argv.indexOf("--shots");
const shots = shotsIndex >= 0 ? process.argv[shotsIndex + 1] : undefined;

const webRoot = process.env.NEXUS_DASHBOARD_WEB_ROOT || join(root, "apps/Nexus-Dashboard/frontend/dist");
if (!existsSync(join(webRoot, "index.html"))) {
  console.error("Build the Dashboard first: cd apps/Nexus-Dashboard/frontend && npm run build");
  process.exit(2);
}

const temp = mkdtempSync(join(tmpdir(), "dashboard-phone-width-"));
const founderPassword = randomBytes(18).toString("base64url");
const authPort = freePort();
const dashPort = freePort();
const closed = "http://127.0.0.1:1";
// Everything a service might read from its .env, set explicitly so the
// production values in those files can never apply here.
const isolation = {
  PATH: process.env.PATH ?? "/usr/bin:/bin",
  HOME: process.env.HOME ?? temp,
  TMPDIR: temp,
  DATABASE_URL: "",
};

const auth = Bun.spawn(["bun", "run", "src/index.ts"], {
  cwd: join(root, "apps/Nexus-Auth"),
  env: {
    ...isolation,
    PORT: String(authPort),
    NEXUS_AUTH_BASE_URL: `http://127.0.0.1:${authPort}`,
    NEXUS_AUTH_USER_STORE_PATH: join(temp, "users.json"),
    NEXUS_AUTH_RECOVERY_STORE_PATH: join(temp, "recovery.json"),
    NEXUS_AUTH_INVITE_STORE_PATH: join(temp, "invites.json"),
    NEXUS_AUTH_JWT_PRIVATE_KEY_FILE: join(temp, "jwt.pem"),
    NEXUS_AUTH_OIDC_CLIENTS_FILE: join(temp, "oidc-clients.json"),
    NEXUS_AUTH_AUDIT_DATABASE_URL: "",
    NEXUS_AUTH_ENABLE_CLOUD_INTEGRATION: "false",
    NEXUS_AUTH_FOUNDER_PASSWORD: founderPassword,
    NEXUS_AUTH_OPERATOR_PASSWORD: randomBytes(18).toString("base64url"),
    NEXUS_EMAIL_SYSTEM_MAIL_TOKEN: "",
    NEXUS_CLOUD_URL: closed,
  },
  stdout: "ignore",
  stderr: "ignore",
});
let dashboard: ReturnType<typeof Bun.spawn> | undefined;

try {
  await waitFor(`http://127.0.0.1:${authPort}/health`, "Auth");
  dashboard = Bun.spawn(["bun", "run", "src/index.ts"], {
    cwd: join(root, "apps/Nexus-Dashboard"),
    env: {
      ...isolation,
      PORT: String(dashPort),
      NEXUS_BIND_HOST: "127.0.0.1",
      DOMAIN: "tnhc.dev",
      NEXUS_AUTH_INTERNAL_URL: `http://127.0.0.1:${authPort}`,
      NEXUS_DASHBOARD_ENABLE_CLOUD_INTEGRATION: "false",
      NEXUS_CLOUD_URL: closed, NEXUS_CLOUD_API_KEY: "",
      NEXUS_EMAIL_URL: closed, NEXUS_TERMINAL_URL: closed, NEXUS_HOSTING_URL: closed,
      NEXUS_CALENDAR_URL: closed, NEXUS_CALENDAR_WEB_URL: closed, NEXUS_CALENDAR_DASHBOARD_SECRET: "",
      NEXUS_ISSUES_TOKEN: "", NEXUS_DHTS_ROOT: join(root, "dhts"),
      NEXUS_DASHBOARD_WEB_ROOT: webRoot,
    },
    stdout: "ignore",
    stderr: "ignore",
  });
  await waitFor(`http://127.0.0.1:${dashPort}/health`, "Dashboard");

  const login = await fetch(`http://127.0.0.1:${authPort}/api/v1/auth/login`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ username: "founder", password: founderPassword }),
  });
  const { token } = (await login.json()) as { token?: string };
  if (!token) throw new Error(`throwaway founder could not sign in (${login.status})`);

  const check = Bun.spawn([
    "bun", join(root, "scripts/check-phone-width.ts"),
    ...(shots ? ["--shots", shots] : []),
    "--cookie", `nexus_session=${token}`,
    ...PAGES.map((p) => `http://127.0.0.1:${dashPort}${p}`),
  ], { stdout: "inherit", stderr: "inherit" });
  process.exitCode = await check.exited;
} finally {
  dashboard?.kill();
  auth.kill();
  await Promise.all([auth.exited, dashboard?.exited]);
  rmSync(temp, { recursive: true, force: true });
}
