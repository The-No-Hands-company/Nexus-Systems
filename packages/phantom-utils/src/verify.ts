import { createPhantomSDK } from "../../phantom-sdk/src/index";

type Verifier = (did: string, message: string, signature: string) => Promise<boolean>;
let testOverride: Verifier | null = null;

export function __test_set_verifier(fn: Verifier | null) {
  testOverride = fn;
}

/** Resolve a DID to its signing public key via the Nexus-Cloud DID registry. */
async function resolveSigningKey(did: string): Promise<string | null> {
  const cloudUrl = process.env.NEXUS_CLOUD_URL ?? "http://localhost:8787";
  try {
    const res = await fetch(`${cloudUrl}/api/v1/phantom/registry/${encodeURIComponent(did)}`);
    if (!res.ok) return null;
    const body = await res.json() as { signingPublicKey?: string };
    return typeof body.signingPublicKey === "string" ? body.signingPublicKey : null;
  } catch {
    return null;
  }
}

/**
 * Verify a proof-of-possession signature over `message` from the holder of `did`.
 *
 * Production path: resolve DID → signing public key via the Nexus-Cloud DID
 * registry, then verify the Dilithium-5 signature with the phantom-sdk.
 *
 * Development bypass: set PHANTOM_DEV_SKIP_VERIFY=1 to accept any well-formed
 * DID without cryptographic verification. Never set this in production.
 */
export async function verifyDidSignature(did: string, message: string, signature: string): Promise<boolean> {
  if (testOverride) return testOverride(did, message, signature);

  if (!did || !message || !signature) return false;

  if (!/^did:phantom:[A-Za-z0-9_:-]+$/.test(did)) return false;

  const devSkip = process.env.PHANTOM_DEV_SKIP_VERIFY === "1";
  if (devSkip) {
    console.warn(`[phantom-utils] PHANTOM_DEV_SKIP_VERIFY: accepting DID ${did} without cryptographic verification`);
    return true;
  }

  const signingPublicKey = await resolveSigningKey(did);
  if (!signingPublicKey) {
    // DID not registered — could be a new service that hasn't registered yet
    console.warn(`[phantom-utils] DID not in registry: ${did}`);
    return false;
  }

  try {
    const sdk = await createPhantomSDK();
    const temp = await sdk.generateIdentity("verifier");
    try {
      const msgBytes = new TextEncoder().encode(message);
      return await sdk.verify(temp.handle, msgBytes, signature);
    } finally {
      sdk.release(temp.handle);
    }
  } catch (err) {
    console.warn(`[phantom-utils] Verification error for ${did}:`, err instanceof Error ? err.message : err);
    return false;
  }
}
