type Verifier = (did: string, message: string, signature: string) => Promise<boolean>;
let testOverride: Verifier | null = null;

export function __test_set_verifier(fn: Verifier | null) {
  testOverride = fn;
}

/**
 * Verify a proof-of-possession signature over `message` from the holder of `did`.
 *
 * Production path (TODO): resolve DID → public key via the DID registry, then
 * use the native phantom-sdk to verify the Dilithium-5 signature. Build the
 * native library at packages/phantom-sdk/wasm and deploy it to enable real
 * post-quantum verification.
 *
 * Development path: set PHANTOM_DEV_SKIP_VERIFY=1 to bypass cryptographic
 * verification for local development. Never set this in production.
 *
 * Default behavior: returns false (reject) until real verification is wired in.
 */
export async function verifyDidSignature(did: string, message: string, signature: string): Promise<boolean> {
  if (testOverride) return testOverride(did, message, signature);

  if (!did || !message || !signature) return false;

  // Development bypass — only when explicitly opted in and DID is well-formed.
  // did:phantom:<method>:<id>
  const devSkip = process.env.PHANTOM_DEV_SKIP_VERIFY === "1";
  if (devSkip && /^did:phantom:[a-z][a-z0-9-]*:[A-Za-z0-9_-]+$/.test(did)) {
    console.warn(`[phantom-utils] PHANTOM_DEV_SKIP_VERIFY: accepting DID ${did} without cryptographic verification`);
    return true;
  }

  // Real verification not yet implemented — reject until phantom-native is deployed.
  return false;
}
