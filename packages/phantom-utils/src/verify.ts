export async function verifyDidSignature(did: string, message: string, signature: string): Promise<boolean> {
  // TODO: Integrate real Phantom SDK here (e.g. packages/phantom-sdk) to verify
  // the proof-of-possession signature. This stub returns true for tests.
  // Replace with actual verification logic before production use.
  return true;
}
