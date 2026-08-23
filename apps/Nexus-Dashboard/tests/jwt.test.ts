import { describe, it, expect } from "bun:test";

/**
 * A real RS256 keypair generated for this suite. The private side stands in
 * for Nexus-Auth's signer; the public side is published as the JWK that
 * fetchJWKS() would return.
 */
const { generateKeyPairSync } = await import("node:crypto");

const { publicKey, privateKey } = generateKeyPairSync("rsa", { modulusLength: 2048 });

function b64url(buf: Buffer): string {
  return buf.toString("base64url");
}

/** Public half of `privateKey`, as an RSA JWK — the shape JWKS serves. */
function publicJwk(): Record<string, string> {
  const jwk = publicKey.export({ format: "jwk" }) as { kty: string; n: string; e: string };
  return { kty: jwk.kty, use: "sig", kid: KID, alg: "RS256", n: jwk.n, e: jwk.e };
}

const KID = "test-key-1";
const now = Math.floor(Date.now() / 1000);

// Point the module under test at a JWKS we control BEFORE it is ever
// imported: jwt.ts captures AUTH_INTERNAL_URL at module load.
const { serve } = await import("bun");
const jwksServer = serve({
  port: 0,
  fetch: () => Response.json({ keys: [publicJwk()] }),
});

process.env.NEXUS_AUTH_INTERNAL_URL = `http://127.0.0.1:${jwksServer.port}`;

// Dynamic import only: a static one would evaluate jwt.ts before the env
// above exists and silently target whatever Auth happens to be running.
const jwtModule = await import("../src/jwt");
const validate = jwtModule.validateJWT;
const JWTValidationError = jwtModule.JWTValidationError;

function sign(payload: Record<string, unknown>, kid = KID): string {
  const { sign: rsaSign } = require("node:crypto") as typeof import("node:crypto");
  const header = b64url(Buffer.from(JSON.stringify({ alg: "RS256", typ: "JWT", kid })));
  const body = b64url(Buffer.from(JSON.stringify(payload)));
  const signingInput = `${header}.${body}`;
  const sig = b64url(rsaSign("RSA-SHA256", Buffer.from(signingInput), privateKey));
  return `${signingInput}.${sig}`;
}

describe("JWT validation", () => {
  it("accepts a token signed by the trusted key with matching audience", async () => {
    const token = sign({
      iss: "nexus-auth",
      sub: "user-1",
      aud: "nexus-dashboard",
      role: "founder",
      iat: now,
      exp: now + 300,
      jti: "jti-1",
    });
    const payload = await validate(token);
    expect(payload.sub).toBe("user-1");
    expect(payload.role).toBe("founder");
  });

  it("rejects a tampered payload without touching expiry logic", async () => {
    const token = sign({ iss: "nexus-auth", sub: "user-1", aud: "nexus-dashboard", exp: now + 300 });
    const [h, , s] = token.split(".");
    const forgedBody = b64url(Buffer.from(JSON.stringify({ iss: "nexus-auth", sub: "attacker", aud: "nexus-dashboard", exp: now + 9999 })));
    await expect(validate(`${h}.${forgedBody}.${s}`)).rejects.toThrow(JWTValidationError);
  });

  it("rejects expired tokens even when the signature is valid", async () => {
    const token = sign({ iss: "nexus-auth", sub: "user-1", aud: "nexus-dashboard", exp: now - 10 });
    try {
      await validate(token);
      throw new Error("should have thrown");
    } catch (err) {
      expect((err as JWTValidationError).code).toBe("TOKEN_EXPIRED");
    }
  });

  it("rejects wrong-audience tokens when an audience is expected", async () => {
    const token = sign({ iss: "nexus-auth", sub: "user-1", aud: "other-service", exp: now + 300 });
    try {
      await validate(token, "nexus-dashboard");
      throw new Error("should have thrown");
    } catch (err) {
      expect((err as JWTValidationError).code).toBe("AUDIENCE_MISMATCH");
    }
  });

  it("rejects unknown-key tokens rather than trusting any kid", async () => {
    const token = sign({ iss: "nexus-auth", sub: "user-1", aud: "nexus-dashboard", exp: now + 300 }, "attacker-key");
    try {
      await validate(token);
      throw new Error("should have thrown");
    } catch (err) {
      expect((err as JWTValidationError).code).toBe("KEY_NOT_FOUND");
    }
  });

  it("refuses non-RS256 algorithms outright (alg-confusion)", async () => {
    // Hand-build an HS256-looking header; there is no valid signature either way.
    const header = b64url(Buffer.from(JSON.stringify({ alg: "HS256", typ: "JWT" })));
    const body = b64url(Buffer.from(JSON.stringify({ sub: "x" })));
    try {
      await validate(`${header}.${body}.AAAA`);
      throw new Error("should have thrown");
    } catch (err) {
      expect((err as JWTValidationError).code).toBe("UNSUPPORTED_ALGORITHM");
    }
  });

  it("rejects malformed tokens before doing any crypto", async () => {
    try {
      await validate("not-a-jwt");
      throw new Error("should have thrown");
    } catch (err) {
      expect((err as JWTValidationError).code).toBe("INVALID_FORMAT");
    }
  });
});