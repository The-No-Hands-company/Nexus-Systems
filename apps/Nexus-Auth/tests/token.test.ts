import { describe, it, expect, beforeEach } from "bun:test";
import * as users from "../src/users";
import { issueIdToken } from "../src/oidc";

describe("ID token claims", () => {
  beforeEach(() => users.clearUsers());

  it("includes phantom_did claim when user has one", () => {
    const PASSWORD = "pw-phantom-12345"; // pragma: allowlist secret
    const u = users.createUser({ username: "phantom", email: "p@x.dev", password: PASSWORD, phantom_did: "did:phantom:abc" });

    const token = issueIdToken({ userId: u.id, clientId: "client-1", extraClaims: { preferred_username: u.username } });
    const parts = token.split('.');
    expect(parts.length).toBe(3);
    const payload = JSON.parse(Buffer.from(parts[1], 'base64url').toString('utf8'));
    expect(payload.phantom_did).toBe('did:phantom:abc');
  });
});
