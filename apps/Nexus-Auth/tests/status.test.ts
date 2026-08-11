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
