import { describe, it, expect, beforeEach } from "bun:test";
import * as users from "../src/users";
import * as recovery from "../src/recovery";
import * as invites from "../src/invites";

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
