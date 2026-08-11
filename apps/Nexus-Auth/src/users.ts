import { createHash, randomBytes, scryptSync, timingSafeEqual } from "node:crypto";
import type { User, IdentityRole, Permission, AccountStatus } from "./types";
import { mkdirSync, readFileSync, writeFileSync, renameSync, rmSync, dirname, resolve, join, fileURLToPath } from "./types";

import { issueRecoveryCodes } from "./recovery";

export type SafeUser = Omit<User, "passwordHash" | "totpSecret" | "claimCodeHash">;

const users = new Map<string, User>();
let userCounter = 0;

const stateFilePath = resolve(
  process.env.NEXUS_AUTH_USER_STORE_PATH ||
    join(dirname(fileURLToPath(import.meta.url)), "..", "data", "auth-users.json"),
);

type PersistedUsers = {
  version: number;
  counter: number;
  users: User[];
  updatedAt: string;
};

function hashPassword(password: string): string {
  const salt = randomBytes(16).toString("hex");
  const hash = scryptSync(password, salt, 64).toString("hex");
  return `${salt}:${hash}`;
}

function verifyPassword(password: string, storedHash: string): boolean {
  const [salt, hash] = storedHash.split(":");
  if (!salt || !hash) return false;
  const computed = scryptSync(password, salt, 64).toString("hex");
  try {
    return timingSafeEqual(Buffer.from(computed), Buffer.from(hash));
  } catch {
    return false;
  }
}

function generateUserId(): string {
  return `usr-${Date.now().toString(36)}-${(++userCounter).toString(36)}`;
}

function hashToken(token: string): string {
  return createHash("sha256").update(token).digest("hex");
}

function persistUsers(): void {
  const payload: PersistedUsers = {
    version: 1,
    counter: userCounter,
    users: Array.from(users.values()),
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

function hydrateUsers(): void {
  try {
    const raw = readFileSync(stateFilePath, "utf-8");
    const parsed = JSON.parse(raw) as Partial<PersistedUsers>;
    users.clear();
    for (const user of Array.isArray(parsed.users) ? parsed.users : []) {
      if (user && typeof user.id === "string" && typeof user.username === "string") {
        // Records written before the status machine existed carry `disabled`
        // instead. Map them so an existing deployment does not lock everyone out.
        if (typeof (user as { status?: string }).status !== "string") {
          user.status = (user as unknown as { disabled?: boolean }).disabled ? "suspended" : "active";
        }
        users.set(user.id, user);
      }
    }
    userCounter = Math.max(0, Number(parsed.counter || 0));
  } catch {
    persistUsers();
  }
}

export function createUser(input: {
  username: string;
  email: string;
  password: string;
  role?: IdentityRole;
}): SafeUser {
  if (findUserByUsername(input.username)) {
    throw new Error(`Username '${input.username}' already exists`);
  }

  const now = new Date().toISOString();
  const user: User = {
    id: generateUserId(),
    username: input.username.trim().toLowerCase(),
    email: input.email.trim().toLowerCase(),
    role: input.role || "user",
    passwordHash: hashPassword(input.password),
    totpEnabled: false,
    status: "active",
    createdAt: now,
    updatedAt: now,
  };

  users.set(user.id, user);
  persistUsers();
  return sanitizeUser(user);
}

/** 16 random bytes as 32 hex chars — 128 bits, far beyond guessing. */
function generateCode(): string {
  return randomBytes(16).toString("hex");
}

/**
 * Public self-service access request. Creates a `pending` account and returns a
 * one-time claim code that is shown to the requester immediately and never
 * stored in plaintext or transmitted again.
 *
 * The code is handed over now rather than at approval time because there is no
 * outbound email to deliver it with later: the user polls, and this code is
 * what proves the returning visitor is the person who made the request. Without
 * it, anyone who guessed an approved email address could seize the account.
 *
 * passwordHash is deliberately empty. The account cannot authenticate in any
 * case — authenticateUser requires status 'active' — and claimAccount sets a
 * real hash before the account ever reaches that state.
 */
export function createAccessRequest(input: {
  username: string;
  email: string;
  note?: string;
}): { user: SafeUser; claimCode: string } {
  const username = input.username.trim().toLowerCase();
  const email = input.email.trim().toLowerCase();

  if (findUserByUsername(username)) throw new Error(`Username '${username}' already exists`);
  if (findUserByEmail(email)) throw new Error(`Email '${email}' already exists`);

  const claimCode = generateCode();
  const now = new Date().toISOString();
  const user: User = {
    id: generateUserId(),
    username,
    email,
    role: "user",
    passwordHash: "",
    claimCodeHash: hashToken(claimCode),
    totpEnabled: false,
    status: "pending",
    createdAt: now,
    updatedAt: now,
  };
  if (input.note) user.note = input.note.trim().slice(0, 500);

  users.set(user.id, user);
  persistUsers();
  return { user: sanitizeUser(user), claimCode };
}

/** Shortest password accepted. Long enough to matter, short enough to type. */
export const MIN_PASSWORD_LENGTH = 12;

/**
 * Redeems a claim code, sets the password and activates the account.
 *
 * An unknown email and a wrong code both return `invalid_code` on purpose: a
 * distinct "no such account" would let an attacker enumerate which addresses
 * have been approved.
 */
export function claimAccount(input: {
  email: string;
  claimCode: string;
  password: string;
}): { ok: true; user: SafeUser; recoveryCodes: string[] } | { ok: false; reason: string } {
  const user = findUserByEmail(input.email);

  // Same answer for "no such user", "already claimed" and "wrong code".
  if (!user || !user.claimCodeHash) return { ok: false, reason: "invalid_code" };

  const supplied = hashToken(input.claimCode);
  let matches = false;
  try {
    matches = timingSafeEqual(Buffer.from(supplied), Buffer.from(user.claimCodeHash));
  } catch {
    matches = false;
  }
  if (!matches) return { ok: false, reason: "invalid_code" };

  // Code was right, so the caller is the requester — now it is safe to be
  // specific about why this cannot proceed.
  if (user.status !== "approved") return { ok: false, reason: "not_approved" };
  if (input.password.length < MIN_PASSWORD_LENGTH) return { ok: false, reason: "weak_password" };

  user.passwordHash = hashPassword(input.password);
  delete user.claimCodeHash;
  user.status = "active";
  user.updatedAt = new Date().toISOString();

  users.set(user.id, user);
  persistUsers();

  // Issued here, shown once by the caller, never retrievable again.
  const recoveryCodes = issueRecoveryCodes(user.id);
  return { ok: true, user: sanitizeUser(user), recoveryCodes };
}

/** Accounts in a given lifecycle state — the operator's approval queue. */
export function listByStatus(status: AccountStatus): SafeUser[] {
  return Array.from(users.values())
    .filter((u) => u.status === status)
    .map(sanitizeUser);
}

export function authenticateUser(username: string, password: string): SafeUser | null {
  const user = findUserByUsername(username);
  // Only a fully claimed, unsuspended account may authenticate. pending and
  // approved accounts have no usable password yet.
  if (!user || user.status !== "active") return null;

  if (!verifyPassword(password, user.passwordHash)) return null;

  user.lastLoginAt = new Date().toISOString();
  users.set(user.id, user);
  persistUsers();

  return sanitizeUser(user);
}

export function findUserByUsername(username: string): User | undefined {
  const normalized = username.trim().toLowerCase();
  for (const user of users.values()) {
    if (user.username === normalized) return user;
  }
  return undefined;
}

export function findUserByEmail(email: string): User | undefined {
  const normalized = email.trim().toLowerCase();
  for (const user of users.values()) {
    if (user.email === normalized) return user;
  }
  return undefined;
}

export function getUser(userId: string): SafeUser | undefined {
  const user = users.get(userId);
  return user ? sanitizeUser(user) : undefined;
}

export function listUsers(): SafeUser[] {
  return Array.from(users.values()).map(sanitizeUser);
}

export function updateUser(userId: string, patch: {
  email?: string;
  role?: IdentityRole;
  status?: AccountStatus;
}): SafeUser | undefined {
  const user = users.get(userId);
  if (!user) return undefined;

  if (patch.email !== undefined) user.email = patch.email.trim().toLowerCase();
  if (patch.role !== undefined) user.role = patch.role;
  if (patch.status !== undefined) user.status = patch.status;
  user.updatedAt = new Date().toISOString();

  users.set(user.id, user);
  persistUsers();
  return sanitizeUser(user);
}

/**
 * Moves an account to a new lifecycle state. `actorId` is recorded only for the
 * approval transition, which is the one an operator is accountable for.
 */
export function setUserStatus(userId: string, status: AccountStatus, actorId?: string): SafeUser | undefined {
  const user = users.get(userId);
  if (!user) return undefined;

  user.status = status;
  if (status === "approved") {
    user.approvedAt = new Date().toISOString();
    if (actorId) user.approvedBy = actorId;
  }
  user.updatedAt = new Date().toISOString();

  users.set(user.id, user);
  persistUsers();
  return sanitizeUser(user);
}

export function changePassword(userId: string, currentPassword: string, newPassword: string): boolean {
  const user = users.get(userId);
  if (!user) return false;
  if (!verifyPassword(currentPassword, user.passwordHash)) return false;

  user.passwordHash = hashPassword(newPassword);
  user.updatedAt = new Date().toISOString();
  users.set(user.id, user);
  persistUsers();
  return true;
}

export function deleteUser(userId: string): boolean {
  const deleted = users.delete(userId);
  if (deleted) persistUsers();
  return deleted;
}

export function userHasPermission(userId: string, permission: Permission): boolean {
  const user = users.get(userId);
  if (!user || user.status !== "active") return false;

  const rolePermissions: Record<IdentityRole, Permission[]> = {
    founder: [
      "auth:admin", "auth:read", "users:create", "users:read", "users:update", "users:delete", "users:approve",
      "tokens:issue", "tokens:revoke", "tokens:validate",
      "apikeys:create", "apikeys:revoke", "apikeys:read",
      "sessions:read", "sessions:revoke", "system:health", "system:config",
    ],
    admin: [
      "auth:read", "users:create", "users:read", "users:update", "users:delete", "users:approve",
      "tokens:issue", "tokens:revoke", "tokens:validate",
      "apikeys:create", "apikeys:revoke", "apikeys:read",
      "sessions:read", "sessions:revoke", "system:health", "system:config",
    ],
    operator: [
      "auth:read", "users:read", "tokens:validate",
      "apikeys:read", "sessions:read", "system:health",
    ],
    user: ["auth:read", "system:health"],
    viewer: ["auth:read"],
  };

  return (rolePermissions[user.role] || []).includes(permission);
}

export function sanitizeUser(user: User): SafeUser {
  const { passwordHash, totpSecret, claimCodeHash, ...safe } = user;
  return safe;
}

export function seedDefaultUsers(adminUsername?: string, operatorUsername?: string): SafeUser[] {
  const results: SafeUser[] = [];
  const admin = adminUsername?.trim().toLowerCase() || "founder";
  const operator = operatorUsername?.trim().toLowerCase() || "operator";

  if (!findUserByUsername(admin)) {
    results.push(createUser({
      username: admin,
      email: `${admin}@nexus.local`,
      password: process.env.NEXUS_AUTH_FOUNDER_PASSWORD || "nexus-founder-2026",
      role: "founder",
    }));
  }

  if (!findUserByUsername(operator)) {
    results.push(createUser({
      username: operator,
      email: `${operator}@nexus.local`,
      password: process.env.NEXUS_AUTH_OPERATOR_PASSWORD || "nexus-operator-2026",
      role: "operator",
    }));
  }

  return results;
}

export function clearUsers(): void {
  users.clear();
  userCounter = 0;
  persistUsers();
}

hydrateUsers();
