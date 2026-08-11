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

import { handleRequest } from "../src/server";
import { validateServiceToken } from "../src/token";

const BASE = "http://auth.test";

async function sessionToken(username: string): Promise<string> {
  users.createUser({ username, email: `${username}@x.dev`, password: PASSWORD });
  const res = await handleRequest(new Request(`${BASE}/api/v1/auth/login`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ username, password: PASSWORD }),
  }));
  return (await res.json() as { token: string }).token;
}

function mint(audience: unknown, headers: Record<string, string> = {}) {
  return handleRequest(new Request(`${BASE}/api/v1/auth/identity-token`, {
    method: "POST",
    headers: { "content-type": "application/json", ...headers },
    body: JSON.stringify({ audience }),
  }));
}

describe("identity-token endpoint", () => {
  it("refuses an unauthenticated caller", async () => {
    users.clearUsers();
    expect((await mint("chat.tnhc.dev")).status).toBe(401);
  });

  it("mints a token a JWKS consumer can verify, scoped to the audience", async () => {
    users.clearUsers();
    const session = await sessionToken("erin");
    const res = await mint("chat.tnhc.dev", { authorization: `Bearer ${session}` });
    expect(res.status).toBe(200);

    const { token } = await res.json() as { token: string };
    const ok = validateServiceToken(token, "chat.tnhc.dev");
    expect(ok.valid).toBe(true);
    if (ok.valid) expect(ok.payload.sub).toBe(users.findUserByUsername("erin")!.id);
  });

  it("rejects the token against a different audience", async () => {
    users.clearUsers();
    const session = await sessionToken("finn");
    const { token } = await (await mint("chat.tnhc.dev", {
      authorization: `Bearer ${session}`,
    })).json() as { token: string };

    // This is the replay case: a token minted for chat must not satisfy draw.
    expect(validateServiceToken(token, "draw.tnhc.dev").valid).toBe(false);
  });

  it("requires an audience", async () => {
    users.clearUsers();
    const session = await sessionToken("gus");
    expect((await mint("", { authorization: `Bearer ${session}` })).status).toBe(400);
    expect((await mint(42, { authorization: `Bearer ${session}` })).status).toBe(400);
  });

  it("refuses a suspended account", async () => {
    users.clearUsers();
    const session = await sessionToken("hal");
    users.setUserStatus(users.findUserByUsername("hal")!.id, "suspended");
    expect((await mint("chat.tnhc.dev", { authorization: `Bearer ${session}` })).status).toBe(403);
  });
});
