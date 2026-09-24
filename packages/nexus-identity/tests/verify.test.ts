import { describe, it, expect, beforeAll, afterAll, beforeEach } from "bun:test";
import { generateKeyPairSync, createSign, randomUUID } from "node:crypto";
import {
  verifyIdentityToken,
  identityFromRequest,
  __resetIdentityCacheForTest,
  __jwksFetchCountForTest,
  IDENTITY_HEADER,
  IDENTITY_ISSUER,
  IDENTITY_TYP,
} from "../src/index";

const AUDIENCE = "calendar.tnhc.dev";
const KID = "auth-key-1";
const { publicKey, privateKey } = generateKeyPairSync("rsa", { modulusLength: 2048 });

let jwksServer: ReturnType<typeof Bun.serve>;
let jwksUrl = "";

function b64url(v: string): string {
  return Buffer.from(v).toString("base64url");
}

/**
 * Signs an arbitrary claim set with Auth's key. Defaults produce a valid
 * identity token; each override reproduces one thing Auth really does or one
 * thing an attacker would try.
 */
function mint(claims: Record<string, unknown> = {}, over: { alg?: string; kid?: string; tamper?: boolean } = {}): string {
  const now = Math.floor(Date.now() / 1000);
  const header = { alg: over.alg ?? "RS256", kid: over.kid ?? KID, typ: "JWT" };
  const payload = {
    iss: IDENTITY_ISSUER,
    sub: "usr-alice",
    aud: AUDIENCE,
    email: "alice@tnhc.dev",
    username: "alice",
    role: "user",
    typ: IDENTITY_TYP,
    iat: now - 10,
    exp: now + 120,
    jti: randomUUID(),
    ...claims,
  };
  const signingInput = `${b64url(JSON.stringify(header))}.${b64url(JSON.stringify(payload))}`;
  const signer = createSign("RSA-SHA256");
  signer.update(signingInput);
  signer.end();
  let sig = signer.sign(privateKey);
  if (over.tamper) sig = Buffer.concat([sig.subarray(0, sig.length - 1), Buffer.from([sig[sig.length - 1]! ^ 0xff])]);
  return `${signingInput}.${sig.toString("base64url")}`;
}

/**
 * Exactly what Nexus-Auth's issueServiceToken() produces: same key, same kid,
 * caller-chosen `sub` and `aud`, role "service" — and no `typ` claim at all.
 */
function mintServiceToken(serviceId: string, audience: string): string {
  const now = Math.floor(Date.now() / 1000);
  const header = { alg: "RS256", kid: KID, typ: "JWT" };
  const payload = {
    iss: "nexus-auth",
    sub: serviceId,
    aud: audience,
    scopes: ["service:read"],
    role: "service",
    iat: now,
    exp: now + 3600,
    jti: randomUUID(),
  };
  const signingInput = `${b64url(JSON.stringify(header))}.${b64url(JSON.stringify(payload))}`;
  const signer = createSign("RSA-SHA256");
  signer.update(signingInput);
  signer.end();
  return `${signingInput}.${signer.sign(privateKey).toString("base64url")}`;
}

beforeAll(() => {
  const jwk = publicKey.export({ format: "jwk" }) as { n: string; e: string };
  jwksServer = Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch(req) {
      if (new URL(req.url).pathname === "/api/v1/auth/oauth/jwks") {
        return Response.json({ keys: [{ kty: "RSA", use: "sig", alg: "RS256", kid: KID, n: jwk.n, e: jwk.e }] });
      }
      return new Response("not found", { status: 404 });
    },
  });
  jwksUrl = `http://127.0.0.1:${jwksServer.port}/api/v1/auth/oauth/jwks`;
});
afterAll(() => jwksServer.stop(true));
beforeEach(() => __resetIdentityCacheForTest());

const opts = () => ({ audience: AUDIENCE, jwksUrl });

describe("a real identity token", () => {
  it("verifies and returns its claims", async () => {
    const r = await verifyIdentityToken(mint(), opts());
    expect(r.ok).toBe(true);
    if (r.ok) {
      expect(r.claims.sub).toBe("usr-alice");
      expect(r.claims.role).toBe("user");
      expect(r.claims.typ).toBe(IDENTITY_TYP);
    }
  });

  it("accepts an audience array containing ours", async () => {
    const r = await verifyIdentityToken(mint({ aud: ["other.tnhc.dev", AUDIENCE] }), opts());
    expect(r.ok).toBe(true);
  });
});

describe("a service token is not a user", () => {
  // The reason this package exists. Auth signs service tokens with the SAME key
  // and the same kid, and lets the caller choose both `sub` and `aud`. Checking
  // only the signature and the audience — which both TypeScript services did —
  // means anyone able to issue a service token can name themselves any user.
  // chat's Rust verifier has always rejected these; nothing else did.
  it("refuses a service token aimed at this audience", async () => {
    const r = await verifyIdentityToken(mintServiceToken("usr-victim", AUDIENCE), opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("wrong_type");
  });

  it("refuses any token whose typ is not exactly identity", async () => {
    for (const typ of ["service", "refresh", "access", "IDENTITY", "", null]) {
      const r = await verifyIdentityToken(mint({ typ }), opts());
      expect(r.ok).toBe(false);
      if (!r.ok) expect(r.reason).toBe("wrong_type");
    }
  });
});

describe("the issuer must be Auth", () => {
  it.each(["someone-else", "", null])("refuses iss %p", async (iss) => {
    const r = await verifyIdentityToken(mint({ iss }), opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("wrong_issuer");
  });
});

describe("the audience must be this service", () => {
  it("refuses a token minted for another app", async () => {
    const r = await verifyIdentityToken(mint({ aud: "chat.tnhc.dev" }), opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("wrong_audience");
  });

  it("refuses a token with no audience at all", async () => {
    const r = await verifyIdentityToken(mint({ aud: undefined }), opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("wrong_audience");
  });
});

describe("signature and algorithm", () => {
  it.each(["none", "HS256", "RS512", ""])("refuses alg %p", async (alg) => {
    const r = await verifyIdentityToken(mint({}, { alg }), opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("unsupported_algorithm");
  });

  it("refuses a tampered signature", async () => {
    const r = await verifyIdentityToken(mint({}, { tamper: true }), opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("bad_signature");
  });

  it("refuses a kid the JWKS does not publish", async () => {
    const r = await verifyIdentityToken(mint({}, { kid: "nope" }), opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("unknown_kid");
  });

  it.each(["", "abc", "a.b", "a.b.c.d"])("refuses the malformed token %p", async (bad) => {
    const r = await verifyIdentityToken(bad, opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("malformed");
  });
});

describe("time", () => {
  it("refuses an expired token", async () => {
    const now = Math.floor(Date.now() / 1000);
    const r = await verifyIdentityToken(mint({ exp: now - 1 }), opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("expired");
  });

  it("refuses a token issued implausibly far in the future", async () => {
    const now = Math.floor(Date.now() / 1000);
    const r = await verifyIdentityToken(mint({ iat: now + 600 }), opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("not_yet_valid");
  });

  it("refuses a token with no expiry", async () => {
    const r = await verifyIdentityToken(mint({ exp: undefined }), opts());
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("expired");
  });
});

describe("the JWKS cache cannot be used as an amplifier", () => {
  it("does not refetch for every unknown kid", async () => {
    // The header is unauthenticated when it is read, so an attacker picks the
    // kid. Without a floor between refreshes that is one fetch to Auth per
    // request, from anyone, unauthenticated.
    await verifyIdentityToken(mint(), opts());       // one fetch, populates cache
    const after = __jwksFetchCountForTest();
    for (let i = 0; i < 20; i++) {
      await verifyIdentityToken(mint({}, { kid: `bogus-${i}` }), opts());
    }
    expect(__jwksFetchCountForTest() - after).toBeLessThanOrEqual(1);
  });

  it("serves repeat verifications from cache", async () => {
    await verifyIdentityToken(mint(), opts());
    const after = __jwksFetchCountForTest();
    await verifyIdentityToken(mint(), opts());
    await verifyIdentityToken(mint(), opts());
    expect(__jwksFetchCountForTest()).toBe(after);
  });

  it("reports jwks_unavailable rather than accepting when Auth is unreachable", async () => {
    const r = await verifyIdentityToken(mint(), { audience: AUDIENCE, jwksUrl: "http://127.0.0.1:1/nope" });
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("jwks_unavailable");
  });
});

describe("identityFromRequest", () => {
  it("reads the agreed header name", async () => {
    const req = new Request("http://svc/api", { headers: { [IDENTITY_HEADER]: mint() } });
    const claims = await identityFromRequest(req, opts());
    expect(claims?.sub).toBe("usr-alice");
  });

  it("returns null when the header is absent", async () => {
    expect(await identityFromRequest(new Request("http://svc/api"), opts())).toBeNull();
  });

  it("returns null for a service token", async () => {
    const req = new Request("http://svc/api", {
      headers: { [IDENTITY_HEADER]: mintServiceToken("usr-victim", AUDIENCE) },
    });
    expect(await identityFromRequest(req, opts())).toBeNull();
  });
});
