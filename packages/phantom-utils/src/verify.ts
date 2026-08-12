type Verifier = (did: string, message: string, signature: string) => Promise<boolean>;
let testOverride: Verifier | null = null;

export function __test_set_verifier(fn: Verifier | null) {
  testOverride = fn;
}

export async function verifyDidSignature(did: string, message: string, signature: string): Promise<boolean> {
  if (testOverride) return testOverride(did, message, signature);

  // TODO: Integrate real Phantom SDK here (e.g. packages/phantom-sdk) to verify
  // the proof-of-possession signature. In production, resolve DID -> public key
  // and use the WASM-backed SDK to verify. Returning false by default until
  // real verification is wired.
  return false;
}
