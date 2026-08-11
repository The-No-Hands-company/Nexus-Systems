import { describe, it, expect, beforeEach } from "bun:test";
import * as users from "../src/users";
import * as recovery from "../src/recovery";

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
