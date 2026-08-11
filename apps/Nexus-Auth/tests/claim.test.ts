import { describe, it, expect, beforeEach } from "bun:test";
import * as users from "../src/users";

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
