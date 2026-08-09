import {
  createHash,
  createPrivateKey,
  createPublicKey,
  generateKeyPairSync,
  type KeyObject,
} from "node:crypto";
import { mkdirSync, readFileSync, writeFileSync, existsSync, chmodSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

/**
 * Asymmetric signing keys for service tokens.
 *
 * Service tokens were signed HS256 with NEXUS_AUTH_TOKEN_SECRET, and the JWKS
 * endpoint published that secret as the `k` member of an `oct` key. JWKS is
 * public by design — its entire purpose is letting anyone fetch verification
 * material — but with a symmetric algorithm the verification key *is* the
 * signing key. Any unauthenticated caller could read it and mint tokens for the
 * whole ecosystem. On auth.tnhc.dev that endpoint would be open to the internet.
 *
 * With RS256 the private key never leaves this service and JWKS carries only the
 * public half, which is what a verifier actually needs. It also fixes the shape
 * problem for self-hosting: an app can verify tokens locally without being
 * handed the power to issue them.
 *
 * RSA rather than Ed25519 for interoperability — `kty: "RSA"` is understood by
 * every JWT library, while OKP/Ed25519 support is still patchy, and this is a
 * contract third parties are meant to integrate against.
 *
 * Key material resolution, in order:
 *   1. NEXUS_AUTH_JWT_PRIVATE_KEY      — PEM inline (secret managers, CI)
 *   2. NEXUS_AUTH_JWT_PRIVATE_KEY_FILE — PEM on disk (mounted secret)
 *   3. data/jwt-private-key.pem        — generated on first run and persisted
 *
 * Persisting matters: regenerating per boot would invalidate every issued token
 * on restart. The generated file is written 0600.
 */

const KEY_ALG = "RS256";

function defaultKeyPath(): string {
  return resolve(
    process.env.NEXUS_AUTH_JWT_PRIVATE_KEY_FILE ||
      join(dirname(fileURLToPath(import.meta.url)), "..", "data", "jwt-private-key.pem"),
  );
}

function loadOrCreatePrivatePem(): string {
  const inline = process.env.NEXUS_AUTH_JWT_PRIVATE_KEY?.trim();
  if (inline) return inline;

  const path = defaultKeyPath();
  if (existsSync(path)) return readFileSync(path, "utf8");

  const { privateKey } = generateKeyPairSync("rsa", { modulusLength: 2048 });
  const pem = privateKey.export({ type: "pkcs8", format: "pem" }).toString();
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, pem, { mode: 0o600 });
  try {
    chmodSync(path, 0o600);
  } catch {
    // Best effort: some filesystems (e.g. a mounted volume) refuse chmod.
  }
  console.warn(
    `[nexus-auth] generated a new RS256 signing key at ${path} — ` +
      "back this up, or set NEXUS_AUTH_JWT_PRIVATE_KEY, or every issued token " +
      "becomes invalid if this file is lost",
  );
  return pem;
}

const privatePem = loadOrCreatePrivatePem();
const privateKeyObject: KeyObject = createPrivateKey(privatePem);
const publicKeyObject: KeyObject = createPublicKey(privateKeyObject);

/**
 * Key id derived from the public key itself (RFC 7638 thumbprint-style), so it
 * changes when the key does and a client can tell one from the other.
 */
function computeKid(jwk: { n: string; e: string }): string {
  const canonical = JSON.stringify({ e: jwk.e, kty: "RSA", n: jwk.n });
  return createHash("sha256").update(canonical).digest("base64url");
}

const publicJwkBase = publicKeyObject.export({ format: "jwk" }) as { n: string; e: string };
const kid = computeKid(publicJwkBase);

export const signingAlgorithm = KEY_ALG;
export const signingKid = kid;
export const privateKey = privateKeyObject;
export const publicKey = publicKeyObject;

/** The public half only. Safe to serve unauthenticated — that is the point. */
export function publicJwk(): Record<string, unknown> {
  return {
    kty: "RSA",
    use: "sig",
    alg: KEY_ALG,
    kid,
    n: publicJwkBase.n,
    e: publicJwkBase.e,
  };
}
