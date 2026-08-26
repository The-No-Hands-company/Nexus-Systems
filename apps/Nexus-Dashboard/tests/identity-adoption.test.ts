import { describe, it, expect, beforeAll, afterAll, beforeEach } from "bun:test";
import { generateKeyPairSync, createSign, randomUUID } from "node:crypto";
import { callerIdentity } from "../src/auth";
import { __resetIdentityCacheForTest } from "../../../packages/nexus-identity/src/index";

// Must match the other proxy tests: src/server.ts captures AUTH_INTERNAL_URL
// into a top-level const the first time it is imported in a `bun test` run, so
// every test file sharing that module graph has to agree on the value. Setting
// an ephemeral port here made four unrelated tests in server.test.ts fail
// depending on which file bun loaded first.
process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:4399";

const AUDIENCE = "app.tnhc.dev";
const KID = "auth-key-1";
const { publicKey, privateKey } = generateKeyPairSync("rsa", { modulusLength: 2048 });

let authServer: ReturnType<typeof Bun.serve>;
let savedAudience: string | undefined;

function b64url(v: string): string {
  return Buffer.from(v).toString("base64url");
}

function signClaims(payload: Record<string, unknown>): string {
  const header = { alg: "RS256", kid: KID, typ: "JWT" };
  const signingInput = `${b64url(JSON.stringify(header))}.${b64url(JSON.stringify(payload))}`;
  const signer = createSign("RSA-SHA256");
  signer.update(signingInput);
  signer.end();
  return `${signingInput}.${signer.sign(privateKey).toString("base64url")}`;
}

/** What Auth's buildIdentityClaims() produces. */
function identityToken(over: Record<string, unknown> = {}): string {
  const now = Math.floor(Date.now() / 1000);
  return signClaims({
    iss: "nexus-auth", sub: "usr-alice", aud: AUDIENCE,
    email: "alice@tnhc.dev", username: "alice", role: "user",
    typ: "identity", iat: now - 10, exp: now + 120, jti: randomUUID(),
    ...over,
  });
}

/** What Auth's issueServiceToken() produces: same key, no typ, caller-chosen sub/aud. */
function serviceToken(serviceId: string, audience: string): string {
  const now = Math.floor(Date.now() / 1000);
  return signClaims({
    iss: "nexus-auth", sub: serviceId, aud: audience,
    scopes: ["service:read"], role: "service", iat: now, exp: now + 3600, jti: randomUUID(),
  });
}

beforeAll(() => {
  const jwk = publicKey.export({ format: "jwk" }) as { n: string; e: string };
  authServer = Bun.serve({
    port: 4399,
    hostname: "127.0.0.1",
    fetch(req) {
      const p = new URL(req.url).pathname;
      if (p === "/api/v1/auth/oauth/jwks") {
        return Response.json({ keys: [{ kty: "RSA", use: "sig", alg: "RS256", kid: KID, n: jwk.n, e: jwk.e }] });
      }
      // No session: the cookie fallback must not rescue a refused token.
      if (p === "/api/v1/auth/me") return Response.json({ error: "unauthenticated" }, { status: 401 });
      return new Response("not found", { status: 404 });
    },
  });
  savedAudience = process.env.NEXUS_DASHBOARD_IDENTITY_AUDIENCE;
  process.env.NEXUS_DASHBOARD_IDENTITY_AUDIENCE = AUDIENCE;
});

afterAll(() => {
  authServer.stop(true);
  if (savedAudience === undefined) Reflect.deleteProperty(process.env, "NEXUS_DASHBOARD_IDENTITY_AUDIENCE");
  else process.env.NEXUS_DASHBOARD_IDENTITY_AUDIENCE = savedAudience;
});

beforeEach(() => __resetIdentityCacheForTest());

const withIdentity = (token: string) =>
  new Request("http://app.test/ipa/me", { headers: { "x-nexus-identity": token } });

describe("Dashboard reads the proxy's identity header", () => {
  it("accepts a real identity token and carries its role", async () => {
    const who = await callerIdentity(withIdentity(identityToken()));
    expect(who).toEqual({ subject: "usr-alice", role: "user" });
  });

  it("carries an admin role through unchanged", async () => {
    const who = await callerIdentity(withIdentity(identityToken({ role: "founder" })));
    expect(who?.role).toBe("founder");
  });
});

describe("Dashboard refuses what the shared contract refuses", () => {
  // The reason the contract was extracted. Auth signs service tokens with the
  // same key and lets whoever calls it choose both `sub` and `aud`, so a
  // verifier that checks only signature-plus-audience treats one as the user
  // its `sub` names. Anyone holding tokens:issue — founder or admin — could
  // therefore act as any user.
  it("refuses a service token aimed at this host", async () => {
    expect(await callerIdentity(withIdentity(serviceToken("usr-victim", AUDIENCE)))).toBeNull();
  });

  it("refuses a token minted for another app", async () => {
    expect(await callerIdentity(withIdentity(identityToken({ aud: "calendar.tnhc.dev" })))).toBeNull();
  });

  it("refuses an expired token", async () => {
    const now = Math.floor(Date.now() / 1000);
    expect(await callerIdentity(withIdentity(identityToken({ exp: now - 5 })))).toBeNull();
  });

  it("refuses a token from another issuer", async () => {
    expect(await callerIdentity(withIdentity(identityToken({ iss: "somewhere-else" })))).toBeNull();
  });

  it("refuses a forged header that is not a token at all", async () => {
    for (const junk of ["usr-victim", "Bearer usr-victim", "a.b.c"]) {
      expect(await callerIdentity(withIdentity(junk))).toBeNull();
    }
  });

  it("does not fall through to an ambient cookie when the token is refused", async () => {
    // Otherwise a refused token would be indistinguishable from no token, and
    // the weaker credential would silently decide the answer.
    const req = new Request("http://app.test/ipa/me", {
      headers: {
        "x-nexus-identity": serviceToken("usr-victim", AUDIENCE),
        cookie: "nexus_session=whatever",
      },
    });
    expect(await callerIdentity(req)).toBeNull();
  });
});
