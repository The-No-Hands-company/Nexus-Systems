import { createHash, randomBytes } from "node:crypto";
import { mkdirSync, readFileSync, writeFileSync, renameSync, rmSync, dirname, resolve, join, fileURLToPath } from "./types";
import { createUser, findUserByEmail, findUserByUsername, MIN_PASSWORD_LENGTH, type SafeUser } from "./users";
import { issueRecoveryCodes } from "./recovery";

/**
 * Operator-minted invite codes — the direct path in, handed over in whatever
 * channel the operator already uses. Redeeming creates an active account
 * immediately, skipping the pending/approved queue that the public waitlist
 * goes through.
 */

const DEFAULT_EXPIRY_DAYS = 30;

type Invite = {
  id: string;
  codeHash: string;
  createdBy: string;
  createdAt: string;
  expiresAt: string;
  redeemedBy?: string;
  redeemedAt?: string;
};

type PersistedInvites = { version: number; invites: Invite[]; updatedAt: string };

const invites = new Map<string, Invite>();

const stateFilePath = resolve(
  process.env.NEXUS_AUTH_INVITE_STORE_PATH ||
    join(dirname(fileURLToPath(import.meta.url)), "..", "data", "auth-invites.json"),
);

function hashCode(code: string): string {
  return createHash("sha256").update(code).digest("hex");
}

function persist(): void {
  const payload: PersistedInvites = {
    version: 1,
    invites: Array.from(invites.values()),
    updatedAt: new Date().toISOString(),
  };
  const tempPath = `${stateFilePath}.tmp`;
  try {
    mkdirSync(dirname(stateFilePath), { recursive: true });
    writeFileSync(tempPath, JSON.stringify(payload, null, 2), "utf-8");
    renameSync(tempPath, stateFilePath);
  } catch {
    try { rmSync(tempPath, { force: true }); } catch {}
  }
}

function hydrate(): void {
  try {
    const parsed = JSON.parse(readFileSync(stateFilePath, "utf-8")) as Partial<PersistedInvites>;
    invites.clear();
    for (const i of Array.isArray(parsed.invites) ? parsed.invites : []) {
      if (i && typeof i.id === "string" && typeof i.codeHash === "string") invites.set(i.id, i);
    }
  } catch {
    persist();
  }
}

export function createInvite(createdBy: string, expiresInDays = DEFAULT_EXPIRY_DAYS): {
  id: string; code: string; expiresAt: string;
} {
  const code = randomBytes(16).toString("hex");
  const now = new Date();
  const expiresAt = new Date(now.getTime() + expiresInDays * 86_400_000).toISOString();
  const invite: Invite = {
    id: `inv-${now.getTime().toString(36)}-${randomBytes(4).toString("hex")}`,
    codeHash: hashCode(code),
    createdBy,
    createdAt: now.toISOString(),
    expiresAt,
  };
  invites.set(invite.id, invite);
  persist();
  return { id: invite.id, code, expiresAt };
}

/**
 * Redeems an invite into a new active account.
 *
 * Validation order matters: the invite is only marked redeemed once the account
 * has actually been created. A rejected attempt — weak password, taken
 * username — must leave the code usable, or a typo would burn someone's only
 * way in.
 */
export function redeemInvite(input: {
  code: string;
  username: string;
  email: string;
  password: string;
}): { ok: true; user: SafeUser; recoveryCodes: string[] } | { ok: false; reason: string } {
  const supplied = hashCode(input.code);
  const invite = Array.from(invites.values()).find((i) => i.codeHash === supplied);

  // Unknown and already-redeemed are the same answer: neither should confirm
  // that a code ever existed.
  if (!invite || invite.redeemedBy) return { ok: false, reason: "invalid_code" };
  if (Date.parse(invite.expiresAt) < Date.now()) return { ok: false, reason: "expired" };
  if (input.password.length < MIN_PASSWORD_LENGTH) return { ok: false, reason: "weak_password" };

  const username = input.username.trim().toLowerCase();
  const email = input.email.trim().toLowerCase();
  if (findUserByUsername(username)) return { ok: false, reason: "username_taken" };
  if (findUserByEmail(email)) return { ok: false, reason: "email_taken" };

  const user = createUser({ username, email, password: input.password, role: "user" });

  invite.redeemedBy = username;
  invite.redeemedAt = new Date().toISOString();
  invites.set(invite.id, invite);
  persist();

  return { ok: true, user, recoveryCodes: issueRecoveryCodes(user.id) };
}

export function listInvites(): Array<Omit<Invite, "codeHash">> {
  return Array.from(invites.values()).map(({ codeHash, ...rest }) => rest);
}

export function clearInviteStore(): void {
  invites.clear();
  persist();
}

hydrate();
