export type Mapping = { did: string; user_id: string; created_at?: string };

const didToUser = new Map<string, Mapping>();
const userToDid = new Map<string, Mapping>();

export async function getUserByDid(did: string): Promise<Mapping | null> {
  return didToUser.get(did) ?? null;
}

export async function getDidByUserId(user_id: string): Promise<Mapping | null> {
  return userToDid.get(user_id) ?? null;
}

export async function createMapping(mapping: Mapping): Promise<Mapping> {
  if (didToUser.has(mapping.did)) throw new Error('DID_EXISTS');
  didToUser.set(mapping.did, mapping);
  userToDid.set(mapping.user_id, mapping);
  return mapping;
}

export async function deleteMapping(did: string): Promise<boolean> {
  const m = didToUser.get(did);
  if (!m) return false;
  didToUser.delete(did);
  userToDid.delete(m.user_id);
  return true;
}

// helper for tests
export function clearStore(): void {
  didToUser.clear();
  userToDid.clear();
}
