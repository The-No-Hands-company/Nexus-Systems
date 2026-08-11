import { describe, it, expect } from "bun:test";
import { buildIdentityClaims, IDENTITY_TOKEN_TTL_SECONDS } from "../src/identity";
import * as users from "../src/users";

const PASSWORD = "correct-horse-battery-staple";  // pragma: allowlist secret

describe("identity claims", () => {
  it("carries who the user is and which app the token is for", () => {
    users.clearUsers();
    const u = users.createUser({ username: "ada", email: "ada@x.dev", password: PASSWORD });
    const c = buildIdentityClaims(u, "chat.tnhc.dev", 1_000_000);

    expect(c.iss).toBe("nexus-auth");
    expect(c.sub).toBe(u.id);
    expect(c.aud).toBe("chat.tnhc.dev");
    expect(c.email).toBe("ada@x.dev");
    expect(c.username).toBe("ada");
    expect(c.role).toBe("user");
    expect(c.typ).toBe("identity");
  });

  it("expires in two minutes", () => {
    users.clearUsers();
    const u = users.createUser({ username: "bo", email: "bo@x.dev", password: PASSWORD });
    const c = buildIdentityClaims(u, "chat.tnhc.dev", 1_000_000);
    expect(c.iat).toBe(1_000_000);
    expect(c.exp).toBe(1_000_000 + IDENTITY_TOKEN_TTL_SECONDS);
    expect(IDENTITY_TOKEN_TTL_SECONDS).toBe(120);
  });

  it("gives every token a distinct id", () => {
    users.clearUsers();
    const u = users.createUser({ username: "cy", email: "cy@x.dev", password: PASSWORD });
    const a = buildIdentityClaims(u, "chat.tnhc.dev");
    const b = buildIdentityClaims(u, "chat.tnhc.dev");
    expect(a.jti).not.toBe(b.jti);
  });

  it("never leaks a password hash into the claims", () => {
    users.clearUsers();
    const u = users.createUser({ username: "di", email: "di@x.dev", password: PASSWORD });
    const c = buildIdentityClaims(u, "chat.tnhc.dev") as Record<string, unknown>;
    expect(JSON.stringify(c)).not.toContain("passwordHash");
    expect(c.passwordHash).toBeUndefined();
  });
});
