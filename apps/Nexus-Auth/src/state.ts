import type { SeedIdentity } from "./types";

const identities = new Map<string, SeedIdentity>();

function upsertSeedIdentity(username: string, role: SeedIdentity["role"]): { identity: SeedIdentity; created: boolean } {
  const existing = identities.get(username);
  if (existing) {
    return { identity: existing, created: false };
  }

  const identity: SeedIdentity = {
    id: `${role}-${username}`,
    username,
    role,
    createdAt: new Date().toISOString(),
  };

  identities.set(username, identity);
  return { identity, created: true };
}

export function seedDefaultIdentities(input?: { adminUsername?: string; userUsername?: string }) {
  const adminUsername = input?.adminUsername?.trim() || "founder";
  const userUsername = input?.userUsername?.trim() || "operator";

  const admin = upsertSeedIdentity(adminUsername, "admin");
  const user = upsertSeedIdentity(userUsername, "user");

  return {
    createdCount: Number(admin.created) + Number(user.created),
    identities: [admin.identity, user.identity],
  };
}

export function listSeedIdentities(): SeedIdentity[] {
  return [...identities.values()];
}
