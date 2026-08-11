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
