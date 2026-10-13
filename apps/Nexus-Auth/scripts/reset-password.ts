// Reset an existing account's password in the Auth user store.
//
//   bun apps/Nexus-Auth/scripts/reset-password.ts [username]   (default: founder)
//
// The seed in src/users.ts only creates accounts that do not exist, so changing
// NEXUS_AUTH_FOUNDER_PASSWORD does nothing to a live store. This is the rotation
// path. Auth must be STOPPED first: it holds users in memory and rewrites the
// store on its next change, which would silently undo this edit. The script
// refuses to run while anything answers on Auth's port.
//
// The password is taken from NEXUS_RESET_PASSWORD if set; otherwise a strong
// random one is generated and printed once. The previous store is kept next to
// it as auth-users.json.bak-<timestamp>.

import { randomBytes, scryptSync } from "node:crypto";
import { copyFileSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const username = (process.argv[2] ?? "founder").trim().toLowerCase();
const authPort = process.env.NEXUS_AUTH_PORT ?? "4310";
const storePath = resolve(
  process.env.NEXUS_AUTH_USER_STORE_PATH ||
    join(dirname(fileURLToPath(import.meta.url)), "..", "data", "auth-users.json"),
);

const running = await fetch(`http://127.0.0.1:${authPort}/health`, { signal: AbortSignal.timeout(2000) })
  .then(() => true, () => false);
if (running) {
  console.error(`Auth is still answering on :${authPort}. Stop it first, or it will overwrite this change:`);
  console.error(`  bash -c 'source deploy/production/deploy.sh; stop_service auth'`);
  process.exit(1);
}

const store = JSON.parse(readFileSync(storePath, "utf8")) as {
  users: Array<{ username: string; role: string; status?: string; totpSecret?: string; passwordHash: string; updatedAt?: string }>;
  updatedAt?: string;
};
const user = store.users.find((u) => u.username.toLowerCase() === username);
if (!user) {
  console.error(`No account named "${username}" in ${storePath}. Accounts: ${store.users.map((u) => u.username).join(", ")}`);
  process.exit(1);
}

const generated = !process.env.NEXUS_RESET_PASSWORD;
const password = process.env.NEXUS_RESET_PASSWORD || randomBytes(18).toString("base64url");
if (password.length < 12) {
  console.error("Password must be at least 12 characters.");
  process.exit(1);
}

// Same format as hashPassword() in src/users.ts: "<hex salt>:<hex scrypt-64>",
// with the hex salt string itself used as the scrypt salt.
const salt = randomBytes(16).toString("hex");
user.passwordHash = `${salt}:${scryptSync(password, salt, 64).toString("hex")}`;
const now = new Date().toISOString();
if ("updatedAt" in user) user.updatedAt = now;
store.updatedAt = now;

const backup = `${storePath}.bak-${now.replace(/[:.]/g, "-")}`;
copyFileSync(storePath, backup);
const temp = `${storePath}.tmp`;
writeFileSync(temp, JSON.stringify(store, null, 2), { encoding: "utf8", mode: 0o600 });
renameSync(temp, storePath);

console.log(`Password reset for "${user.username}" (role ${user.role}, status ${user.status ?? "active"}).`);
if (user.status && user.status !== "active") console.log(`  Note: the account is ${user.status}; it cannot sign in until it is active.`);
if (user.totpSecret) console.log("  Note: this account has two-factor sign-in enabled; you will still need the code.");
if (generated) console.log(`  New password (shown once, store it now): ${password}`);
console.log(`  Previous store kept at ${backup}`);
console.log("Start Auth again: bash deploy/production/deploy.sh bg");
