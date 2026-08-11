# SSO Phase 1 — Identity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give Nexus-Auth a complete account lifecycle — request access, operator approval, claim with a code, recovery codes, invites — so a stranger can obtain a working account without any outbound email.

**Architecture:** Everything lands inside the existing `apps/Nexus-Auth` Bun service. `users.ts` already owns the user `Map` and its JSON persistence, so the account state machine and claim flow go there rather than into a second module that would need to reach into that Map. Recovery codes, invites and rate limiting get their own modules with their own stores. `server.ts` gains public routes (request/claim/recover/redeem) and admin routes (approve/reject/suspend), following its existing flat `if (method && path)` dispatch.

**Tech Stack:** Bun, TypeScript, `bun test`, `node:crypto` (scrypt for passwords, sha256 for high-entropy codes), JSON-file persistence.

**Spec:** `docs/superpowers/specs/2026-08-11-single-sign-on-and-front-door-design.md`

## Global Constraints

- **Runner is `bun test`, importing from `bun:test`.** This project is Bun, not Vite. Do not import from `vitest`.
- **Never let a test touch the real user store.** Every test file sets `process.env.NEXUS_AUTH_USER_STORE_PATH` to a temp dir and then uses `await import(...)`. A plain top-level `import` is hoisted and runs before the env assignment, which would load and overwrite `apps/Nexus-Auth/data/auth-users.json`.
- **Codes are hashed with sha256, passwords with scrypt.** Claim, recovery and invite codes are 128-bit random values, so a fast hash is correct — scrypt's work factor exists to slow guessing of low-entropy human passwords and buys nothing here. Reuse the existing `hashToken` helper.
- **Code entropy is exactly 16 random bytes** (`randomBytes(16)`), rendered as 32 hex characters.
- **Existing roles stay as they are** — `founder | admin | operator | user | viewer`. Do not collapse them.
- **`username` is the handle.** Do not add a `handle` field.
- Type check with `bun run check` (`bunx tsc --noEmit`) before every commit.
- Run from `apps/Nexus-Auth/`.

---

## File Structure

| File | Responsibility |
|---|---|
| `src/types.ts` (modify) | `AccountStatus` type, `User` fields, new `Permission` members |
| `src/users.ts` (modify) | Owns the user Map: status machine, access requests, approval, claim |
| `src/recovery.ts` (create) | Recovery code generation, verification, consumption |
| `src/invites.ts` (create) | Invite code minting and redemption |
| `src/ratelimit.ts` (create) | Fixed-window rate limiting and per-account lockout |
| `src/server.ts` (modify) | HTTP routes for all of the above |
| `tests/*.test.ts` (create) | One test file per module |

Recovery and invites are separate modules because they own separate stores and have no reason to change when user CRUD changes. The account state machine is *not* separated, because it mutates the user Map that `users.ts` keeps module-private — splitting it would mean exporting mutable internals, which is worse than a slightly longer file.

---

### Task 1: Account status state machine

Replaces the `disabled: boolean` flag with a five-state machine. This is the foundation every later task builds on: "can this account log in" must have exactly one answer, and today it is a boolean that cannot express "approved but not yet claimed".

**Files:**
- Modify: `src/types.ts`
- Modify: `src/users.ts`
- Test: `tests/status.test.ts`

**Interfaces:**
- Consumes: nothing (first task)
- Produces:
  - `type AccountStatus = "pending" | "approved" | "active" | "suspended" | "rejected"`
  - `User.status: AccountStatus` (replaces `User.disabled`)
  - `setUserStatus(userId: string, status: AccountStatus, actorId?: string): SafeUser | undefined`
  - `type SafeUser = Omit<User, "passwordHash" | "totpSecret" | "claimCodeHash">`

- [ ] **Step 1: Write the failing test**

Create `tests/status.test.ts`:

```ts
import { describe, it, expect, beforeEach } from "bun:test";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

// Must be set BEFORE importing users.ts — the module resolves its store path
// and hydrates at import time. A static `import` would be hoisted above this.
process.env.NEXUS_AUTH_USER_STORE_PATH = join(
  mkdtempSync(join(tmpdir(), "nexus-auth-status-")),
  "users.json",
);
const users = await import("../src/users");

describe("account status", () => {
  beforeEach(() => users.clearUsers());

  it("new users created directly are active", () => {
    const u = users.createUser({ username: "alice", email: "a@x.dev", password: "pw-alice-12345" });  // pragma: allowlist secret
    expect(u.status).toBe("active");
  });

  it("only an active account can authenticate", () => {
    const u = users.createUser({ username: "bob", email: "b@x.dev", password: "pw-bob-12345" });  // pragma: allowlist secret
    expect(users.authenticateUser("bob", "pw-bob-12345")).not.toBeNull();

    for (const blocked of ["pending", "approved", "suspended", "rejected"] as const) {
      users.setUserStatus(u.id, blocked);
      expect(users.authenticateUser("bob", "pw-bob-12345")).toBeNull();
    }

    users.setUserStatus(u.id, "active");
    expect(users.authenticateUser("bob", "pw-bob-12345")).not.toBeNull();
  });

  it("a suspended account holds no permissions", () => {
    const u = users.createUser({ username: "carol", email: "c@x.dev", password: "pw-carol-12345" });  // pragma: allowlist secret
    expect(users.userHasPermission(u.id, "auth:read")).toBe(true);
    users.setUserStatus(u.id, "suspended");
    expect(users.userHasPermission(u.id, "auth:read")).toBe(false);
  });

  it("setUserStatus on an unknown id returns undefined", () => {
    expect(users.setUserStatus("usr-nope", "active")).toBeUndefined();
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test tests/status.test.ts`
Expected: FAIL — `users.setUserStatus is not a function`.

- [ ] **Step 3: Add the type**

In `src/types.ts`, add above `export interface User`:

```ts
/**
 * Account lifecycle. Replaces the old `disabled` boolean, which could not
 * express "approved but not yet claimed" — the state an invited user sits in
 * between the operator saying yes and the user setting a password.
 *
 *   pending   — access requested, awaiting operator decision
 *   approved  — operator said yes; claim code not yet redeemed
 *   active    — password set, can log in
 *   suspended — operator revoked access; recoverable
 *   rejected  — operator said no; terminal
 */
export type AccountStatus = "pending" | "approved" | "active" | "suspended" | "rejected";
```

In the same file, in `export interface User`, replace the line `disabled: boolean;` with:

```ts
  status: AccountStatus;
  /** sha256 of the one-time claim code. Cleared once redeemed. */
  claimCodeHash?: string;
  /** Free-text reason supplied with an access request. Operator-facing only. */
  note?: string;
  approvedAt?: string;
  approvedBy?: string;
```

Note: `OAuthClient` also has a `disabled: boolean` field. Leave it alone — it is unrelated to account status.

- [ ] **Step 4: Update users.ts**

In `src/users.ts`:

Update the import on line 2 to include the new type:

```ts
import type { User, IdentityRole, Permission, AccountStatus } from "./types";
```

Add the exported alias just below the imports:

```ts
export type SafeUser = Omit<User, "passwordHash" | "totpSecret" | "claimCodeHash">;
```

In `createUser`, replace `disabled: false,` with `status: "active",`.

In `authenticateUser`, replace:

```ts
  if (!user || user.disabled) return null;
```

with:

```ts
  // Only a fully claimed, unsuspended account may authenticate. pending and
  // approved accounts have no usable password yet.
  if (!user || user.status !== "active") return null;
```

In `userHasPermission`, replace:

```ts
  if (!user || user.disabled) return false;
```

with:

```ts
  if (!user || user.status !== "active") return false;
```

In `updateUser`, replace the `disabled` field in the patch type and its assignment:

```ts
export function updateUser(userId: string, patch: {
  email?: string;
  role?: IdentityRole;
  status?: AccountStatus;
}): SafeUser | undefined {
```

and replace `if (patch.disabled !== undefined) user.disabled = patch.disabled;` with:

```ts
  if (patch.status !== undefined) user.status = patch.status;
```

Change every remaining `Omit<User, "passwordHash" | "totpSecret">` return annotation in the file to `SafeUser`, and update `sanitizeUser` to strip the new secret field:

```ts
export function sanitizeUser(user: User): SafeUser {
  const { passwordHash, totpSecret, claimCodeHash, ...safe } = user;
  return safe;
}
```

Add the new function after `updateUser`:

```ts
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
```

Finally, in `hydrateUsers`, accounts persisted before this change have no `status`. Add a default inside the hydration loop, right after the `users.set(user.id, user)` guard opens:

```ts
      if (user && typeof user.id === "string" && typeof user.username === "string") {
        // Records written before the status machine existed carry `disabled`
        // instead. Map them so an existing deployment does not lock everyone out.
        if (typeof (user as { status?: string }).status !== "string") {
          user.status = (user as unknown as { disabled?: boolean }).disabled ? "suspended" : "active";
        }
        users.set(user.id, user);
      }
```

- [ ] **Step 5: Update the one server.ts reference**

`src/server.ts:542` passes `disabled` to `updateUser`. Replace:

```ts
          disabled: typeof body.disabled === "boolean" ? body.disabled : undefined,
```

with:

```ts
          status: typeof body.status === "string" ? (body.status as AccountStatus) : undefined,
```

and add `AccountStatus` to the type import from `./types` at the top of `server.ts`.

- [ ] **Step 6: Run test and typecheck**

Run: `bun test tests/status.test.ts && bun run check`
Expected: 4 tests PASS, tsc clean.

- [ ] **Step 7: Commit**

```bash
git add src/types.ts src/users.ts src/server.ts tests/status.test.ts
git commit -m "feat(auth): account status state machine replaces the disabled flag

A boolean could not express 'approved but not yet claimed' — the state an
invited account sits in between the operator approving it and the user
setting a password. authenticateUser and userHasPermission now require
status 'active', so pending, approved, suspended and rejected accounts are
all refused by one rule rather than several.

Records written before this change are mapped on hydrate (disabled ->
suspended, otherwise active) so an existing store does not lock everyone out."
```

---

### Task 2: Access requests and claim codes

The public entry point. A stranger submits a request and is shown a claim code **once** — that code is the secret proving, later, that the returning visitor is the person who asked.

**Files:**
- Modify: `src/users.ts`
- Test: `tests/access-request.test.ts`

**Interfaces:**
- Consumes: `AccountStatus`, `SafeUser`, `setUserStatus` (Task 1)
- Produces: `createAccessRequest(input: { username: string; email: string; note?: string }): { user: SafeUser; claimCode: string }` — throws `Error` when username or email is taken

- [ ] **Step 1: Write the failing test**

Create `tests/access-request.test.ts`:

```ts
import { describe, it, expect, beforeEach } from "bun:test";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

process.env.NEXUS_AUTH_USER_STORE_PATH = join(
  mkdtempSync(join(tmpdir(), "nexus-auth-request-")),
  "users.json",
);
const users = await import("../src/users");

describe("access requests", () => {
  beforeEach(() => users.clearUsers());

  it("creates a pending account and returns a claim code", () => {
    const { user, claimCode } = users.createAccessRequest({
      username: "dana", email: "d@x.dev", note: "want to try it",
    });
    expect(user.status).toBe("pending");
    expect(user.username).toBe("dana");
    expect(user.note).toBe("want to try it");
    expect(claimCode).toMatch(/^[0-9a-f]{32}$/);
  });

  it("never exposes the claim code hash on the returned user", () => {
    const { user } = users.createAccessRequest({ username: "erin", email: "e@x.dev" });
    expect((user as Record<string, unknown>).claimCodeHash).toBeUndefined();
  });

  it("issues a different claim code each time", () => {
    const a = users.createAccessRequest({ username: "f1", email: "f1@x.dev" }).claimCode;
    const b = users.createAccessRequest({ username: "f2", email: "f2@x.dev" }).claimCode;
    expect(a).not.toBe(b);
  });

  it("a pending account cannot authenticate even with an empty password", () => {
    users.createAccessRequest({ username: "gary", email: "g@x.dev" });
    expect(users.authenticateUser("gary", "")).toBeNull();
  });

  it("rejects a duplicate username or email", () => {
    users.createAccessRequest({ username: "hana", email: "h@x.dev" });
    expect(() => users.createAccessRequest({ username: "hana", email: "other@x.dev" })).toThrow();
    expect(() => users.createAccessRequest({ username: "other", email: "h@x.dev" })).toThrow();
  });

  it("lists pending requests for the operator", () => {
    users.createAccessRequest({ username: "ian", email: "i@x.dev" });
    users.createUser({ username: "active-one", email: "ao@x.dev", password: "pw-active-12345" });  // pragma: allowlist secret
    const pending = users.listByStatus("pending");
    expect(pending).toHaveLength(1);
    expect(pending[0]!.username).toBe("ian");
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test tests/access-request.test.ts`
Expected: FAIL — `users.createAccessRequest is not a function`.

- [ ] **Step 3: Implement**

In `src/users.ts`, add after `createUser`:

```ts
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

/** Accounts in a given lifecycle state — the operator's approval queue. */
export function listByStatus(status: AccountStatus): SafeUser[] {
  return Array.from(users.values())
    .filter((u) => u.status === status)
    .map(sanitizeUser);
}
```

- [ ] **Step 4: Run test and typecheck**

Run: `bun test tests/access-request.test.ts && bun run check`
Expected: 6 tests PASS, tsc clean.

- [ ] **Step 5: Commit**

```bash
git add src/users.ts tests/access-request.test.ts
git commit -m "feat(auth): public access requests with one-time claim codes

The claim code is shown at request time rather than sent on approval, because
there is no outbound email to send it with — the user polls instead of being
notified, and the code is what proves the returning visitor made the request.
Stored as sha256 only; 128 bits of entropy means a fast hash is the right
choice, scrypt's work factor buys nothing against a random 16-byte value."
```

---

### Task 3: Claiming an approved account

Turns an `approved` account into an `active` one by redeeming the claim code and setting a password.

**Files:**
- Modify: `src/users.ts`
- Test: `tests/claim.test.ts`

**Interfaces:**
- Consumes: `createAccessRequest`, `setUserStatus`, `SafeUser` (Tasks 1–2)
- Produces: `claimAccount(input: { email: string; claimCode: string; password: string }): { ok: true; user: SafeUser } | { ok: false; reason: string }`

- [ ] **Step 1: Write the failing test**

Create `tests/claim.test.ts`:

```ts
import { describe, it, expect, beforeEach } from "bun:test";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

process.env.NEXUS_AUTH_USER_STORE_PATH = join(
  mkdtempSync(join(tmpdir(), "nexus-auth-claim-")),
  "users.json",
);
const users = await import("../src/users");

const PASSWORD = "correct-horse-battery-staple";  // pragma: allowlist secret

function requested(username = "jo", email = "jo@x.dev") {
  return users.createAccessRequest({ username, email });
}

describe("claiming an account", () => {
  beforeEach(() => users.clearUsers());

  it("activates an approved account and lets it log in", () => {
    const { user, claimCode } = requested();
    users.setUserStatus(user.id, "approved");

    const result = users.claimAccount({ email: "jo@x.dev", claimCode, password: PASSWORD });
    expect(result.ok).toBe(true);
    expect(users.authenticateUser("jo", PASSWORD)).not.toBeNull();
  });

  it("refuses a claim while the account is still pending", () => {
    const { claimCode } = requested();
    const result = users.claimAccount({ email: "jo@x.dev", claimCode, password: PASSWORD });
    expect(result).toEqual({ ok: false, reason: "not_approved" });
  });

  it("refuses a wrong claim code", () => {
    const { user } = requested();
    users.setUserStatus(user.id, "approved");
    const result = users.claimAccount({
      email: "jo@x.dev", claimCode: "0".repeat(32), password: PASSWORD,
    });
    expect(result).toEqual({ ok: false, reason: "invalid_code" });
  });

  it("burns the claim code — it cannot be reused", () => {
    const { user, claimCode } = requested();
    users.setUserStatus(user.id, "approved");
    expect(users.claimAccount({ email: "jo@x.dev", claimCode, password: PASSWORD }).ok).toBe(true);

    const second = users.claimAccount({ email: "jo@x.dev", claimCode, password: "another-password-1" });  // pragma: allowlist secret
    expect(second.ok).toBe(false);
    // The original password must still work — a failed re-claim must not reset it.
    expect(users.authenticateUser("jo", PASSWORD)).not.toBeNull();
  });

  it("refuses an unknown email without revealing that it is unknown", () => {
    const result = users.claimAccount({
      email: "nobody@x.dev", claimCode: "0".repeat(32), password: PASSWORD,
    });
    expect(result).toEqual({ ok: false, reason: "invalid_code" });
  });

  it("rejects a password shorter than 12 characters", () => {
    const { user, claimCode } = requested();
    users.setUserStatus(user.id, "approved");
    const result = users.claimAccount({ email: "jo@x.dev", claimCode, password: "short" });  // pragma: allowlist secret
    expect(result).toEqual({ ok: false, reason: "weak_password" });
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test tests/claim.test.ts`
Expected: FAIL — `users.claimAccount is not a function`.

- [ ] **Step 3: Implement**

In `src/users.ts`, add after `createAccessRequest`:

```ts
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
}): { ok: true; user: SafeUser } | { ok: false; reason: string } {
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
  return { ok: true, user: sanitizeUser(user) };
}
```

- [ ] **Step 4: Run test and typecheck**

Run: `bun test tests/claim.test.ts && bun run check`
Expected: 6 tests PASS, tsc clean.

- [ ] **Step 5: Commit**

```bash
git add src/users.ts tests/claim.test.ts
git commit -m "feat(auth): claim an approved account with a one-time code

Redeeming sets the password, burns the claim code and activates the account.
Unknown email and wrong code return the same 'invalid_code' so the endpoint
cannot be used to enumerate which addresses have been approved; the status
reason is only revealed once the code has proved the caller is the requester."
```

---

### Task 4: Recovery codes

Ten single-use codes issued at claim time. With no email, this is the only self-service way back into an account.

**Files:**
- Create: `src/recovery.ts`
- Modify: `src/users.ts`
- Test: `tests/recovery.test.ts`

**Interfaces:**
- Consumes: `SafeUser`, `claimAccount` (Tasks 1–3)
- Produces:
  - `issueRecoveryCodes(userId: string): string[]` — 10 plaintext codes, replaces any existing set
  - `consumeRecoveryCode(userId: string, code: string): boolean`
  - `countRemainingRecoveryCodes(userId: string): number`
  - `clearRecoveryStore(): void` (tests)
  - `claimAccount` return type gains `recoveryCodes: string[]`

- [ ] **Step 1: Write the failing test**

Create `tests/recovery.test.ts`:

```ts
import { describe, it, expect, beforeEach } from "bun:test";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const dir = mkdtempSync(join(tmpdir(), "nexus-auth-recovery-"));
process.env.NEXUS_AUTH_USER_STORE_PATH = join(dir, "users.json");
process.env.NEXUS_AUTH_RECOVERY_STORE_PATH = join(dir, "recovery.json");
const users = await import("../src/users");
const recovery = await import("../src/recovery");

describe("recovery codes", () => {
  beforeEach(() => {
    users.clearUsers();
    recovery.clearRecoveryStore();
  });

  it("issues ten distinct codes", () => {
    const codes = recovery.issueRecoveryCodes("usr-1");
    expect(codes).toHaveLength(10);
    expect(new Set(codes).size).toBe(10);
    for (const c of codes) expect(c).toMatch(/^[0-9a-f]{32}$/);
  });

  it("accepts a valid code exactly once", () => {
    const [first] = recovery.issueRecoveryCodes("usr-1");
    expect(recovery.consumeRecoveryCode("usr-1", first!)).toBe(true);
    expect(recovery.consumeRecoveryCode("usr-1", first!)).toBe(false);
    expect(recovery.countRemainingRecoveryCodes("usr-1")).toBe(9);
  });

  it("rejects a code belonging to a different account", () => {
    const [mine] = recovery.issueRecoveryCodes("usr-1");
    recovery.issueRecoveryCodes("usr-2");
    expect(recovery.consumeRecoveryCode("usr-2", mine!)).toBe(false);
  });

  it("rejects an unknown code", () => {
    recovery.issueRecoveryCodes("usr-1");
    expect(recovery.consumeRecoveryCode("usr-1", "f".repeat(32))).toBe(false);
  });

  it("regenerating replaces the previous set entirely", () => {
    const [old] = recovery.issueRecoveryCodes("usr-1");
    recovery.issueRecoveryCodes("usr-1");
    expect(recovery.consumeRecoveryCode("usr-1", old!)).toBe(false);
    expect(recovery.countRemainingRecoveryCodes("usr-1")).toBe(10);
  });

  it("claiming an account hands back ten codes", () => {
    const { user, claimCode } = users.createAccessRequest({ username: "kit", email: "k@x.dev" });
    users.setUserStatus(user.id, "approved");
    const result = users.claimAccount({
      email: "k@x.dev", claimCode, password: "correct-horse-battery-staple",  // pragma: allowlist secret
    });
    expect(result.ok).toBe(true);
    if (result.ok) expect(result.recoveryCodes).toHaveLength(10);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test tests/recovery.test.ts`
Expected: FAIL — cannot resolve `../src/recovery`.

- [ ] **Step 3: Implement the module**

Create `src/recovery.ts`:

```ts
import { createHash, randomBytes, timingSafeEqual } from "node:crypto";
import { mkdirSync, readFileSync, writeFileSync, renameSync, rmSync, dirname, resolve, join, fileURLToPath } from "./types";

/**
 * Single-use account recovery codes.
 *
 * With no outbound email there is no "reset link", so these are the only
 * self-service way back into an account. Ten are issued at claim time, shown
 * once, and stored as sha256 — 128 bits of entropy each, so a fast hash is
 * correct and scrypt would only slow down legitimate verification.
 */

const CODES_PER_ACCOUNT = 10;

type RecoveryRecord = { userId: string; codeHashes: string[] };
type PersistedRecovery = { version: number; records: RecoveryRecord[]; updatedAt: string };

const records = new Map<string, string[]>();

const stateFilePath = resolve(
  process.env.NEXUS_AUTH_RECOVERY_STORE_PATH ||
    join(dirname(fileURLToPath(import.meta.url)), "..", "data", "auth-recovery.json"),
);

function hashCode(code: string): string {
  return createHash("sha256").update(code).digest("hex");
}

function persist(): void {
  const payload: PersistedRecovery = {
    version: 1,
    records: Array.from(records.entries()).map(([userId, codeHashes]) => ({ userId, codeHashes })),
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
    const parsed = JSON.parse(readFileSync(stateFilePath, "utf-8")) as Partial<PersistedRecovery>;
    records.clear();
    for (const r of Array.isArray(parsed.records) ? parsed.records : []) {
      if (r && typeof r.userId === "string" && Array.isArray(r.codeHashes)) {
        records.set(r.userId, r.codeHashes);
      }
    }
  } catch {
    persist();
  }
}

/** Issues a fresh set of ten codes, discarding any that already existed. */
export function issueRecoveryCodes(userId: string): string[] {
  const codes = Array.from({ length: CODES_PER_ACCOUNT }, () => randomBytes(16).toString("hex"));
  records.set(userId, codes.map(hashCode));
  persist();
  return codes;
}

/** Verifies and burns one code. Returns false for unknown, wrong or spent codes. */
export function consumeRecoveryCode(userId: string, code: string): boolean {
  const hashes = records.get(userId);
  if (!hashes || hashes.length === 0) return false;

  const supplied = Buffer.from(hashCode(code));
  const index = hashes.findIndex((stored) => {
    try {
      return timingSafeEqual(supplied, Buffer.from(stored));
    } catch {
      return false;
    }
  });
  if (index === -1) return false;

  hashes.splice(index, 1);
  records.set(userId, hashes);
  persist();
  return true;
}

export function countRemainingRecoveryCodes(userId: string): number {
  return records.get(userId)?.length ?? 0;
}

export function clearRecoveryStore(): void {
  records.clear();
  persist();
}

hydrate();
```

- [ ] **Step 4: Issue codes on claim**

In `src/users.ts`, add the import at the top:

```ts
import { issueRecoveryCodes } from "./recovery";
```

Change `claimAccount`'s signature and success return. Replace:

```ts
}): { ok: true; user: SafeUser } | { ok: false; reason: string } {
```

with:

```ts
}): { ok: true; user: SafeUser; recoveryCodes: string[] } | { ok: false; reason: string } {
```

and replace the final return:

```ts
  return { ok: true, user: sanitizeUser(user) };
```

with:

```ts
  // Issued here, shown once by the caller, never retrievable again.
  const recoveryCodes = issueRecoveryCodes(user.id);
  return { ok: true, user: sanitizeUser(user), recoveryCodes };
```

- [ ] **Step 5: Run the whole suite and typecheck**

Run: `bun test && bun run check`
Expected: all tests PASS (Task 3's claim tests still pass — the success shape gained a field but `ok` is unchanged), tsc clean.

- [ ] **Step 6: Commit**

```bash
git add src/recovery.ts src/users.ts tests/recovery.test.ts
git commit -m "feat(auth): single-use recovery codes issued at claim time

With no outbound email there is no reset link, so ten single-use codes are
the only self-service way back into an account. Stored as sha256 of 128-bit
random values, burned on use, and replaced wholesale on regeneration so a
leaked old set stops working."
```

---

### Task 5: Invite codes

The direct path in: the operator mints a code and hands it over out-of-band. Skips `pending` entirely.

**Files:**
- Create: `src/invites.ts`
- Test: `tests/invites.test.ts`

**Interfaces:**
- Consumes: `createUser`, `setUserStatus`, `SafeUser`, `MIN_PASSWORD_LENGTH` (Tasks 1–3), `issueRecoveryCodes` (Task 4)
- Produces:
  - `createInvite(createdBy: string, expiresInDays?: number): { id: string; code: string; expiresAt: string }`
  - `redeemInvite(input: { code: string; username: string; email: string; password: string }): { ok: true; user: SafeUser; recoveryCodes: string[] } | { ok: false; reason: string }`
  - `listInvites(): Array<{ id: string; createdBy: string; createdAt: string; expiresAt: string; redeemedBy?: string; redeemedAt?: string }>`
  - `clearInviteStore(): void` (tests)

- [ ] **Step 1: Write the failing test**

Create `tests/invites.test.ts`:

```ts
import { describe, it, expect, beforeEach } from "bun:test";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const dir = mkdtempSync(join(tmpdir(), "nexus-auth-invites-"));
process.env.NEXUS_AUTH_USER_STORE_PATH = join(dir, "users.json");
process.env.NEXUS_AUTH_RECOVERY_STORE_PATH = join(dir, "recovery.json");
process.env.NEXUS_AUTH_INVITE_STORE_PATH = join(dir, "invites.json");
const users = await import("../src/users");
const recovery = await import("../src/recovery");
const invites = await import("../src/invites");

const PASSWORD = "correct-horse-battery-staple";  // pragma: allowlist secret

describe("invite codes", () => {
  beforeEach(() => {
    users.clearUsers();
    recovery.clearRecoveryStore();
    invites.clearInviteStore();
  });

  it("mints a code that redeems straight to an active account", () => {
    const { code } = invites.createInvite("usr-operator");
    const result = invites.redeemInvite({ code, username: "lee", email: "l@x.dev", password: PASSWORD });

    expect(result.ok).toBe(true);
    if (result.ok) {
      expect(result.user.status).toBe("active");
      expect(result.recoveryCodes).toHaveLength(10);
    }
    expect(users.authenticateUser("lee", PASSWORD)).not.toBeNull();
  });

  it("refuses a second redemption of the same code", () => {
    const { code } = invites.createInvite("usr-operator");
    invites.redeemInvite({ code, username: "mo", email: "m@x.dev", password: PASSWORD });

    const second = invites.redeemInvite({ code, username: "nel", email: "n@x.dev", password: PASSWORD });
    expect(second).toEqual({ ok: false, reason: "invalid_code" });
    expect(users.findUserByUsername("nel")).toBeUndefined();
  });

  it("refuses an unknown code", () => {
    expect(invites.redeemInvite({
      code: "0".repeat(32), username: "opa", email: "o@x.dev", password: PASSWORD,
    })).toEqual({ ok: false, reason: "invalid_code" });
  });

  it("refuses an expired code", () => {
    const { code } = invites.createInvite("usr-operator", -1);
    expect(invites.redeemInvite({
      code, username: "pat", email: "p@x.dev", password: PASSWORD,
    })).toEqual({ ok: false, reason: "expired" });
  });

  it("refuses a weak password without consuming the invite", () => {
    const { code } = invites.createInvite("usr-operator");
    expect(invites.redeemInvite({
      code, username: "quin", email: "q@x.dev", password: "short",  // pragma: allowlist secret
    })).toEqual({ ok: false, reason: "weak_password" });

    // The invite must survive a rejected attempt.
    expect(invites.redeemInvite({
      code, username: "quin", email: "q@x.dev", password: PASSWORD,
    }).ok).toBe(true);
  });

  it("refuses a taken username without consuming the invite", () => {
    users.createUser({ username: "taken", email: "t@x.dev", password: PASSWORD });
    const { code } = invites.createInvite("usr-operator");

    expect(invites.redeemInvite({
      code, username: "taken", email: "other@x.dev", password: PASSWORD,
    })).toEqual({ ok: false, reason: "username_taken" });

    expect(invites.redeemInvite({
      code, username: "free", email: "other@x.dev", password: PASSWORD,
    }).ok).toBe(true);
  });

  it("records who redeemed an invite", () => {
    const { id, code } = invites.createInvite("usr-operator");
    invites.redeemInvite({ code, username: "rae", email: "r@x.dev", password: PASSWORD });
    const listed = invites.listInvites().find((i) => i.id === id);
    expect(listed?.redeemedBy).toBe("rae");
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test tests/invites.test.ts`
Expected: FAIL — cannot resolve `../src/invites`.

- [ ] **Step 3: Implement**

Create `src/invites.ts`:

```ts
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
```

- [ ] **Step 4: Run the whole suite and typecheck**

Run: `bun test && bun run check`
Expected: all tests PASS, tsc clean.

- [ ] **Step 5: Commit**

```bash
git add src/invites.ts tests/invites.test.ts
git commit -m "feat(auth): operator-minted invite codes

The direct path in, handed over out-of-band, redeeming straight to an active
account and skipping the waitlist queue. A rejected attempt — weak password,
taken username — leaves the code usable, so a typo cannot burn someone's only
way in. Unknown and already-redeemed codes give the same answer."
```

---

### Task 6: Rate limiting and lockout

Claim, recovery and invite codes are the entire secret, so the endpoints that check them must not permit unlimited guesses.

**Files:**
- Create: `src/ratelimit.ts`
- Test: `tests/ratelimit.test.ts`

**Interfaces:**
- Consumes: nothing
- Produces:
  - `checkRateLimit(bucket: string, key: string, opts?: { max?: number; windowMs?: number }): { allowed: true } | { allowed: false; retryAfterMs: number }`
  - `recordFailure(bucket: string, key: string): void`
  - `clearFailures(bucket: string, key: string): void`
  - `resetRateLimits(): void` (tests)

- [ ] **Step 1: Write the failing test**

Create `tests/ratelimit.test.ts`:

```ts
import { describe, it, expect, beforeEach } from "bun:test";
const rl = await import("../src/ratelimit");

describe("rate limiting", () => {
  beforeEach(() => rl.resetRateLimits());

  it("allows attempts up to the limit then blocks", () => {
    for (let i = 0; i < 5; i++) {
      expect(rl.checkRateLimit("claim", "1.2.3.4", { max: 5, windowMs: 60_000 }).allowed).toBe(true);
      rl.recordFailure("claim", "1.2.3.4");
    }
    const blocked = rl.checkRateLimit("claim", "1.2.3.4", { max: 5, windowMs: 60_000 });
    expect(blocked.allowed).toBe(false);
    if (!blocked.allowed) expect(blocked.retryAfterMs).toBeGreaterThan(0);
  });

  it("keeps buckets independent", () => {
    for (let i = 0; i < 5; i++) rl.recordFailure("claim", "1.2.3.4");
    expect(rl.checkRateLimit("claim", "1.2.3.4", { max: 5 }).allowed).toBe(false);
    expect(rl.checkRateLimit("recover", "1.2.3.4", { max: 5 }).allowed).toBe(true);
  });

  it("keeps keys independent", () => {
    for (let i = 0; i < 5; i++) rl.recordFailure("claim", "1.2.3.4");
    expect(rl.checkRateLimit("claim", "5.6.7.8", { max: 5 }).allowed).toBe(true);
  });

  it("a success clears the failure count", () => {
    for (let i = 0; i < 4; i++) rl.recordFailure("claim", "1.2.3.4");
    rl.clearFailures("claim", "1.2.3.4");
    expect(rl.checkRateLimit("claim", "1.2.3.4", { max: 5 }).allowed).toBe(true);
  });

  it("forgets failures once the window has passed", () => {
    for (let i = 0; i < 5; i++) rl.recordFailure("claim", "1.2.3.4");
    expect(rl.checkRateLimit("claim", "1.2.3.4", { max: 5, windowMs: 1 }).allowed).toBe(false);
    Bun.sleepSync(5);
    expect(rl.checkRateLimit("claim", "1.2.3.4", { max: 5, windowMs: 1 }).allowed).toBe(true);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test tests/ratelimit.test.ts`
Expected: FAIL — cannot resolve `../src/ratelimit`.

- [ ] **Step 3: Implement**

Create `src/ratelimit.ts`:

```ts
/**
 * Fixed-window failure counting for endpoints where the request body IS the
 * secret — claim codes, recovery codes and invite codes are all 128-bit values
 * that a determined attacker would otherwise be free to guess at line rate.
 *
 * Only failures count. A caller supplying a correct code is not consuming
 * budget, so a legitimate user is never locked out by their own success.
 *
 * In-memory and per-process: restarting Nexus-Auth clears the counters, and a
 * second node would count separately. Acceptable while there is one node —
 * revisit when the gate is federated.
 */

const DEFAULT_MAX = 5;
const DEFAULT_WINDOW_MS = 15 * 60_000;

type Entry = { count: number; firstFailureAt: number };

const failures = new Map<string, Entry>();

function compositeKey(bucket: string, key: string): string {
  return `${bucket}:${key}`;
}

export function checkRateLimit(
  bucket: string,
  key: string,
  opts?: { max?: number; windowMs?: number },
): { allowed: true } | { allowed: false; retryAfterMs: number } {
  const max = opts?.max ?? DEFAULT_MAX;
  const windowMs = opts?.windowMs ?? DEFAULT_WINDOW_MS;

  const entry = failures.get(compositeKey(bucket, key));
  if (!entry) return { allowed: true };

  const elapsed = Date.now() - entry.firstFailureAt;
  if (elapsed >= windowMs) {
    failures.delete(compositeKey(bucket, key));
    return { allowed: true };
  }
  if (entry.count < max) return { allowed: true };

  return { allowed: false, retryAfterMs: windowMs - elapsed };
}

export function recordFailure(bucket: string, key: string): void {
  const composite = compositeKey(bucket, key);
  const entry = failures.get(composite);
  if (entry) {
    entry.count += 1;
    failures.set(composite, entry);
  } else {
    failures.set(composite, { count: 1, firstFailureAt: Date.now() });
  }
}

export function clearFailures(bucket: string, key: string): void {
  failures.delete(compositeKey(bucket, key));
}

export function resetRateLimits(): void {
  failures.clear();
}
```

- [ ] **Step 4: Run test and typecheck**

Run: `bun test tests/ratelimit.test.ts && bun run check`
Expected: 5 tests PASS, tsc clean.

- [ ] **Step 5: Commit**

```bash
git add src/ratelimit.ts tests/ratelimit.test.ts
git commit -m "feat(auth): failure-counting rate limiter for code endpoints

Claim, recovery and invite codes are the whole secret, so the endpoints that
check them cannot allow unlimited guesses. Only failures consume budget, so a
legitimate user is never locked out by their own successful attempts."
```

---

### Task 7: HTTP routes

Exposes the lifecycle over HTTP. Four public endpoints and four operator endpoints, following the existing dispatch and guard style in `server.ts`.

**Files:**
- Modify: `src/types.ts` (one new permission)
- Modify: `src/users.ts` (grant the permission)
- Modify: `src/server.ts`
- Test: `tests/routes.test.ts`

**Interfaces:**
- Consumes: everything from Tasks 1–6
- Produces (HTTP):
  - `POST /api/v1/auth/access-requests` → `201 { user, claimCode }` (public)
  - `POST /api/v1/auth/claim` → `200 { user, recoveryCodes }` (public)
  - `POST /api/v1/auth/recover` → `200 { user, token }` (public)
  - `POST /api/v1/auth/invites/redeem` → `201 { user, recoveryCodes }` (public)
  - `GET /api/v1/auth/access-requests` → `200 { requests }` (needs `users:approve`)
  - `POST /api/v1/auth/access-requests/:id/approve` → `200 { user }` (needs `users:approve`)
  - `POST /api/v1/auth/access-requests/:id/reject` → `200 { user }` (needs `users:approve`)
  - `POST /api/v1/auth/invites` → `201 { id, code, expiresAt }` (needs `users:create`)

- [ ] **Step 1: Write the failing test**

Create `tests/routes.test.ts`. These drive the exported request handler directly rather than binding a port, so the suite stays fast and parallel-safe.

```ts
import { describe, it, expect, beforeEach } from "bun:test";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const dir = mkdtempSync(join(tmpdir(), "nexus-auth-routes-"));
process.env.NEXUS_AUTH_USER_STORE_PATH = join(dir, "users.json");
process.env.NEXUS_AUTH_RECOVERY_STORE_PATH = join(dir, "recovery.json");
process.env.NEXUS_AUTH_INVITE_STORE_PATH = join(dir, "invites.json");

const users = await import("../src/users");
const recovery = await import("../src/recovery");
const invites = await import("../src/invites");
const rl = await import("../src/ratelimit");
const { handleRequest } = await import("../src/server");

const PASSWORD = "correct-horse-battery-staple";  // pragma: allowlist secret
const BASE = "http://auth.test";

function post(path: string, body: unknown, headers: Record<string, string> = {}) {
  return handleRequest(new Request(`${BASE}${path}`, {
    method: "POST",
    headers: { "content-type": "application/json", ...headers },
    body: JSON.stringify(body),
  }));
}

function get(path: string, headers: Record<string, string> = {}) {
  return handleRequest(new Request(`${BASE}${path}`, { method: "GET", headers }));
}

/** Logs in the seeded founder and returns its session token. */
async function founderToken(): Promise<string> {
  users.createUser({ username: "boss", email: "boss@x.dev", password: PASSWORD, role: "founder" });
  const res = await post("/api/v1/auth/login", { username: "boss", password: PASSWORD });
  const body = await res.json() as { token: string };
  return body.token;
}

describe("identity routes", () => {
  beforeEach(() => {
    users.clearUsers();
    recovery.clearRecoveryStore();
    invites.clearInviteStore();
    rl.resetRateLimits();
  });

  it("accepts a public access request and returns a claim code once", async () => {
    const res = await post("/api/v1/auth/access-requests", {
      username: "sam", email: "s@x.dev", note: "curious",
    });
    expect(res.status).toBe(201);
    const body = await res.json() as { user: { status: string }; claimCode: string };
    expect(body.user.status).toBe("pending");
    expect(body.claimCode).toMatch(/^[0-9a-f]{32}$/);
  });

  it("requires authorisation to list or approve requests", async () => {
    expect((await get("/api/v1/auth/access-requests")).status).toBe(403);
    expect((await post("/api/v1/auth/access-requests/usr-x/approve", {})).status).toBe(403);
  });

  it("walks the full request → approve → claim → login path", async () => {
    const token = await founderToken();
    const auth = { authorization: `Bearer ${token}` };

    const reqRes = await post("/api/v1/auth/access-requests", { username: "tia", email: "t@x.dev" });
    const { user, claimCode } = await reqRes.json() as { user: { id: string }; claimCode: string };

    const listRes = await get("/api/v1/auth/access-requests", auth);
    expect(listRes.status).toBe(200);
    expect((await listRes.json() as { requests: unknown[] }).requests).toHaveLength(1);

    const approveRes = await post(`/api/v1/auth/access-requests/${user.id}/approve`, {}, auth);
    expect(approveRes.status).toBe(200);

    const claimRes = await post("/api/v1/auth/claim", {
      email: "t@x.dev", claimCode, password: PASSWORD,
    });
    expect(claimRes.status).toBe(200);
    const claimed = await claimRes.json() as { recoveryCodes: string[] };
    expect(claimed.recoveryCodes).toHaveLength(10);

    const loginRes = await post("/api/v1/auth/login", { username: "tia", password: PASSWORD });
    expect(loginRes.status).toBe(200);
  });

  it("rejects a bad claim with 400 and does not leak the reason for unknown emails", async () => {
    const res = await post("/api/v1/auth/claim", {
      email: "ghost@x.dev", claimCode: "0".repeat(32), password: PASSWORD,
    });
    expect(res.status).toBe(400);
    expect((await res.json() as { error: string }).error).toBe("invalid_code");
  });

  it("blocks repeated claim guesses from one address", async () => {
    const bad = { email: "ghost@x.dev", claimCode: "0".repeat(32), password: PASSWORD };
    const ip = { "x-forwarded-for": "9.9.9.9" };
    for (let i = 0; i < 5; i++) await post("/api/v1/auth/claim", bad, ip);

    const res = await post("/api/v1/auth/claim", bad, ip);
    expect(res.status).toBe(429);
  });

  it("logs in with a recovery code and burns it", async () => {
    const token = await founderToken();
    const auth = { authorization: `Bearer ${token}` };
    const { user, claimCode } = await (await post("/api/v1/auth/access-requests", {
      username: "uma", email: "u@x.dev",
    })).json() as { user: { id: string }; claimCode: string };
    await post(`/api/v1/auth/access-requests/${user.id}/approve`, {}, auth);
    const { recoveryCodes } = await (await post("/api/v1/auth/claim", {
      email: "u@x.dev", claimCode, password: PASSWORD,
    })).json() as { recoveryCodes: string[] };

    const first = await post("/api/v1/auth/recover", { email: "u@x.dev", code: recoveryCodes[0] });
    expect(first.status).toBe(200);

    const reuse = await post("/api/v1/auth/recover", { email: "u@x.dev", code: recoveryCodes[0] });
    expect(reuse.status).toBe(400);
  });

  it("suspends an account through the existing user route and blocks its login", async () => {
    const token = await founderToken();
    const auth = { authorization: `Bearer ${token}` };
    users.createUser({ username: "wes", email: "w@x.dev", password: PASSWORD });
    expect((await post("/api/v1/auth/login", { username: "wes", password: PASSWORD })).status).toBe(200);

    const target = users.findUserByUsername("wes")!;
    const patch = await handleRequest(new Request(`${BASE}/api/v1/auth/users/${target.id}`, {
      method: "PATCH",
      headers: { "content-type": "application/json", ...auth },
      body: JSON.stringify({ status: "suspended" }),
    }));
    expect(patch.status).toBe(200);

    const denied = await post("/api/v1/auth/login", { username: "wes", password: PASSWORD });
    expect(denied.status).toBe(401);
  });

  it("mints and redeems an invite", async () => {
    const token = await founderToken();
    const res = await post("/api/v1/auth/invites", {}, { authorization: `Bearer ${token}` });
    expect(res.status).toBe(201);
    const { code } = await res.json() as { code: string };

    const redeem = await post("/api/v1/auth/invites/redeem", {
      code, username: "vic", email: "v@x.dev", password: PASSWORD,
    });
    expect(redeem.status).toBe(201);
    expect((await post("/api/v1/auth/login", { username: "vic", password: PASSWORD })).status).toBe(200);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test tests/routes.test.ts`
Expected: FAIL — `handleRequest` is not exported from `../src/server`.

- [ ] **Step 3: Export the handler**

`server.ts` currently builds its routing inside the `Bun.serve` options. Extract the request handler so tests can call it without binding a port. Find the `Bun.serve({ ... fetch(request) { ... } ... })` call and lift the `fetch` body into a named export above it:

```ts
export async function handleRequest(request: Request): Promise<Response> {
  // ...the entire existing fetch body, unchanged...
}
```

then reference it from the server:

```ts
  fetch: handleRequest,
```

Make no behavioural changes in this step — it is a pure extraction so the routes become testable.

- [ ] **Step 4: Add the permission**

In `src/types.ts`, add `"users:approve"` to the `Permission` union, after `"users:delete"`.

In `src/users.ts`, add `"users:approve"` to the `founder` and `admin` arrays in `rolePermissions`. Leave `operator`, `user` and `viewer` without it — approving accounts is an owner-level action.

- [ ] **Step 5: Add the routes**

In `src/server.ts`, add these imports:

```ts
import {
  createAccessRequest, claimAccount, listByStatus, setUserStatus, getUser,
} from "./users";
import { consumeRecoveryCode, countRemainingRecoveryCodes } from "./recovery";
import { createInvite, redeemInvite } from "./invites";
import { checkRateLimit, recordFailure, clearFailures } from "./ratelimit";
```

(Merge with the existing `./users` import rather than duplicating it.)

Insert the following inside `handleRequest`, next to the other `/api/v1/auth/...` routes:

```ts
      // ── Public: request access ──
      if (request.method === "POST" && path === "/api/v1/auth/access-requests") {
        const body = await parseBody(request);
        const username = typeof body.username === "string" ? body.username.trim() : "";
        const email = typeof body.email === "string" ? body.email.trim() : "";
        const note = typeof body.note === "string" ? body.note : undefined;

        if (!username || !email) {
          return jsonResponse({ error: "username and email are required" }, { status: 400 });
        }
        try {
          const result = createAccessRequest({ username, email, note });
          // claimCode is returned exactly once, here. It is never stored in
          // plaintext and there is no way to retrieve it again.
          return jsonResponse(result, { status: 201 });
        } catch (err) {
          return jsonResponse({ error: (err as Error).message }, { status: 409 });
        }
      }

      // ── Public: claim an approved account ──
      if (request.method === "POST" && path === "/api/v1/auth/claim") {
        const ip = getClientIp(request);
        const limit = checkRateLimit("claim", ip);
        if (!limit.allowed) {
          return jsonResponse({ error: "too_many_attempts" }, {
            status: 429,
            headers: { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) },
          });
        }

        const body = await parseBody(request);
        const result = claimAccount({
          email: typeof body.email === "string" ? body.email : "",
          claimCode: typeof body.claimCode === "string" ? body.claimCode : "",
          password: typeof body.password === "string" ? body.password : "",  // pragma: allowlist secret
        });

        if (!result.ok) {
          recordFailure("claim", ip);
          return jsonResponse({ error: result.reason }, { status: 400 });
        }
        clearFailures("claim", ip);
        return jsonResponse({ user: result.user, recoveryCodes: result.recoveryCodes });
      }

      // ── Public: log in with a recovery code ──
      if (request.method === "POST" && path === "/api/v1/auth/recover") {
        const ip = getClientIp(request);
        const limit = checkRateLimit("recover", ip);
        if (!limit.allowed) {
          return jsonResponse({ error: "too_many_attempts" }, {
            status: 429,
            headers: { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) },
          });
        }

        const body = await parseBody(request);
        const email = typeof body.email === "string" ? body.email : "";
        const code = typeof body.code === "string" ? body.code : "";
        const user = findUserByEmail(email);

        if (!user || user.status !== "active" || !consumeRecoveryCode(user.id, code)) {
          recordFailure("recover", ip);
          return jsonResponse({ error: "invalid_code" }, { status: 400 });
        }
        clearFailures("recover", ip);

        const session = createSession({
          userId: user.id,
          ipAddress: ip,
          userAgent: request.headers.get("user-agent") || "",
        });
        return jsonResponse({
          user: sanitizeUser(user),
          token: session.token,
          remainingRecoveryCodes: countRemainingRecoveryCodes(user.id),
        });
      }

      // ── Public: redeem an invite ──
      if (request.method === "POST" && path === "/api/v1/auth/invites/redeem") {
        const ip = getClientIp(request);
        const limit = checkRateLimit("invite", ip);
        if (!limit.allowed) {
          return jsonResponse({ error: "too_many_attempts" }, {
            status: 429,
            headers: { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) },
          });
        }

        const body = await parseBody(request);
        const result = redeemInvite({
          code: typeof body.code === "string" ? body.code : "",
          username: typeof body.username === "string" ? body.username : "",
          email: typeof body.email === "string" ? body.email : "",
          password: typeof body.password === "string" ? body.password : "",  // pragma: allowlist secret
        });

        if (!result.ok) {
          recordFailure("invite", ip);
          return jsonResponse({ error: result.reason }, { status: 400 });
        }
        clearFailures("invite", ip);
        return jsonResponse({ user: result.user, recoveryCodes: result.recoveryCodes }, { status: 201 });
      }

      // ── Operator: the approval queue ──
      if (request.method === "GET" && path === "/api/v1/auth/access-requests") {
        if (!auth || !requirePermission(auth.userId, "users:approve")) {
          return jsonResponse({ error: "forbidden" }, { status: 403 });
        }
        return jsonResponse({ requests: listByStatus("pending") });
      }

      // ── Operator: approve / reject ──
      const decision = path.match(/^\/api\/v1\/auth\/access-requests\/([^/]+)\/(approve|reject)$/);
      if (request.method === "POST" && decision) {
        if (!auth || !requirePermission(auth.userId, "users:approve")) {
          return jsonResponse({ error: "forbidden" }, { status: 403 });
        }
        const [, targetId, action] = decision;
        const target = getUser(targetId!);
        if (!target) return jsonResponse({ error: "not_found" }, { status: 404 });
        if (target.status !== "pending") {
          return jsonResponse({ error: "not_pending" }, { status: 409 });
        }

        const updated = setUserStatus(
          targetId!,
          action === "approve" ? "approved" : "rejected",
          auth.userId,
        );
        return jsonResponse({ user: updated });
      }

      // ── Operator: mint an invite ──
      if (request.method === "POST" && path === "/api/v1/auth/invites") {
        if (!auth || !requirePermission(auth.userId, "users:create")) {
          return jsonResponse({ error: "forbidden" }, { status: 403 });
        }
        const body = await parseBody(request);
        const days = typeof body.expiresInDays === "number" ? body.expiresInDays : undefined;
        return jsonResponse(createInvite(auth.userId, days), { status: 201 });
      }
```

If `findUserByEmail`, `sanitizeUser` or `createSession` are not already imported in `server.ts`, add them — from `./users` and `./sessions` respectively.

- [ ] **Step 6: Run the whole suite and typecheck**

Run: `bun test && bun run check`
Expected: all tests PASS across all six files, tsc clean.

- [ ] **Step 7: Verify against the running service**

```bash
bun run src/index.ts &
sleep 2
curl -s -X POST localhost:4310/api/v1/auth/access-requests \
  -H 'content-type: application/json' \
  -d '{"username":"smoke","email":"smoke@x.dev","note":"manual check"}'
```

Expected: `201` with a `user.status` of `"pending"` and a 32-hex `claimCode`. Stop the process afterwards.

- [ ] **Step 8: Commit**

```bash
git add src/types.ts src/users.ts src/server.ts tests/routes.test.ts
git commit -m "feat(auth): HTTP routes for the account lifecycle

Four public endpoints (request access, claim, recover, redeem invite) and
four operator endpoints (queue, approve, reject, mint invite). The three
that check a secret are rate limited by client IP, counting only failures.

server.ts's fetch body is extracted to an exported handleRequest so routes
can be tested without binding a port."
```

---

## Definition of done

- `bun test` green across all six test files.
- `bun run check` clean.
- The full path works against a running instance: request access → approve → claim → log in.
- No route returns a claim, recovery or invite code except the single response that issues it.

## Deliberately not in this plan

These belong to later phases of the spec and must not be built here:

- The proxy gate, route auth policy, and the identity JWT (Phase 2).
- Any UI, including the request and claim forms (Phase 3 — `Nexus-Dashboard`).
- nexus-chat integration, port rebinding, JWT secret rotation (Phase 4).
- TOTP. The recovery-code table is designed to be reused by it, but enrolment is out of scope.
- Auto-approval / open registration. `setUserStatus(id, "approved")` already exists, so the later flag has somewhere to call.
