import { describe, it, expect, beforeAll, afterAll, beforeEach } from "bun:test";
import { generateKeyPairSync, createSign, randomUUID } from "node:crypto";
import { resolveCaller, __resetAuthCacheForTest } from "../src/auth";

const AUDIENCE = "calendar.tnhc.dev";
const SECRET = "s".repeat(64);

const { publicKey, privateKey } = generateKeyPairSync("rsa", { modulusLength: 2048 });
const KID = "test-key-1";

function b64url(input: Buffer | string): string {
  return Buffer.from(input).toString("base64url");
}

/** Mints a token the way Auth does, with knobs for each thing that must fail. */
function mintToken(over: {
  sub?: string; aud?: string; exp?: number; alg?: string; kid?: string; tamper?: boolean;
} = {}): string {
  const header = { alg: over.alg ?? "RS256", kid: over.kid ?? KID, typ: "JWT" };
  const payload = {
    iss: "https://auth.tnhc.dev",
    sub: over.sub ?? "usr-alice",
    aud: over.aud ?? AUDIENCE,
    iat: Math.floor(Date.now() / 1000) - 10,
    exp: over.exp ?? Math.floor(Date.now() / 1000) + 120,
    jti: randomUUID(),
  };
  const signingInput = `${b64url(JSON.stringify(header))}.${b64url(JSON.stringify(payload))}`;
  const signer = createSign("RSA-SHA256");
  signer.update(signingInput);
  signer.end();
  let sig = signer.sign(privateKey);
  if (over.tamper) sig = Buffer.concat([sig.subarray(0, sig.length - 1), Buffer.from([sig[sig.length - 1]! ^ 0xff])]);
  return `${signingInput}.${sig.toString("base64url")}`;
}

// A stand-in for Auth serving its JWKS.
let jwksServer: ReturnType<typeof Bun.serve>;

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
  process.env.NEXUS_AUTH_INTERNAL_URL = `http://127.0.0.1:${jwksServer.port}`;
  process.env.NEXUS_CALENDAR_JWT_AUDIENCE = AUDIENCE;
});

afterAll(() => jwksServer.stop(true));
beforeEach(() => {
  __resetAuthCacheForTest();
  process.env.NEXUS_CALENDAR_DASHBOARD_SECRET = SECRET;
});

function req(headers: Record<string, string>): Request {
  return new Request("http://127.0.0.1/api/v1/calendar/events", { headers });
}

describe("the browser is never a source of identity", () => {
  it("ignores x-nexus-subject with no hop secret at all", async () => {
    // The whole shape of the old bug: anyone who could reach the service could
    // name themselves.
    expect(await resolveCaller(req({ "x-nexus-subject": "usr-victim" }))).toBeNull();
  });

  it("ignores x-nexus-subject with a wrong hop secret", async () => {
    expect(await resolveCaller(req({
      "x-nexus-subject": "usr-victim",
      "x-nexus-dashboard-secret": "w".repeat(64),
    }))).toBeNull();
  });

  it("ignores x-nexus-subject when the secret is a prefix of the real one", async () => {
    expect(await resolveCaller(req({
      "x-nexus-subject": "usr-victim",
      "x-nexus-dashboard-secret": SECRET.slice(0, 32),
    }))).toBeNull();
  });

  it("refuses the hop entirely when no secret is configured", async () => {
    // Otherwise an unset variable turns into "" and an attacker sending "" wins.
    delete process.env.NEXUS_CALENDAR_DASHBOARD_SECRET;
    expect(await resolveCaller(req({
      "x-nexus-subject": "usr-victim",
      "x-nexus-dashboard-secret": "",
    }))).toBeNull();
  });

  it("returns null for a request carrying nothing", async () => {
    expect(await resolveCaller(req({}))).toBeNull();
  });
});

describe("the trusted Dashboard hop", () => {
  it("accepts a subject presented with the correct secret", async () => {
    const who = await resolveCaller(req({
      "x-nexus-subject": "usr-alice",
      "x-nexus-dashboard-secret": SECRET,
    }));
    expect(who).toEqual({ subject: "usr-alice" });
  });

  it("rejects a blank subject even with the correct secret", async () => {
    expect(await resolveCaller(req({
      "x-nexus-subject": "   ",
      "x-nexus-dashboard-secret": SECRET,
    }))).toBeNull();
  });
});

describe("the proxy identity token", () => {
  it("resolves sub from a valid token", async () => {
    const who = await resolveCaller(req({ "x-nexus-identity": mintToken({ sub: "usr-bob" }) }));
    expect(who).toEqual({ subject: "usr-bob" });
  });

  it("rejects a token minted for another audience", async () => {
    expect(await resolveCaller(req({ "x-nexus-identity": mintToken({ aud: "chat.tnhc.dev" }) }))).toBeNull();
  });

  it("rejects an expired token", async () => {
    expect(await resolveCaller(req({
      "x-nexus-identity": mintToken({ exp: Math.floor(Date.now() / 1000) - 5 }),
    }))).toBeNull();
  });

  it("rejects alg:none — the classic downgrade", async () => {
    expect(await resolveCaller(req({ "x-nexus-identity": mintToken({ alg: "none" }) }))).toBeNull();
  });

  it("rejects a symmetric algorithm claim", async () => {
    expect(await resolveCaller(req({ "x-nexus-identity": mintToken({ alg: "HS256" }) }))).toBeNull();
  });

  it("rejects a tampered signature", async () => {
    expect(await resolveCaller(req({ "x-nexus-identity": mintToken({ tamper: true }) }))).toBeNull();
  });

  it("rejects a token signed by a key the JWKS does not publish", async () => {
    expect(await resolveCaller(req({ "x-nexus-identity": mintToken({ kid: "unknown-kid" }) }))).toBeNull();
  });

  it("rejects a structurally malformed token", async () => {
    for (const bad of ["", "abc", "a.b", "a.b.c.d", "...."]) {
      expect(await resolveCaller(req({ "x-nexus-identity": bad }))).toBeNull();
    }
  });
});
