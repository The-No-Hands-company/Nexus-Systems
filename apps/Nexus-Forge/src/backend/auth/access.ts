import type { AccessLevel, ForgeDB, RepositoryRecord, UserRecord } from "../storage/db";

export type Action = "read" | "write" | "admin";

const RANK: Record<AccessLevel, number> = { read: 1, write: 2, admin: 3 };

/**
 * The one access decision in the forge. Every route that touches a
 * repository asks this function and nothing else; there is no second code
 * path that grants access, and anything not granted here is denied.
 *
 *   - anonymous callers may only read public repositories
 *   - a signed-in caller gets the level of their grant, plus read on public
 *     repositories
 */
export function authorize(
  db: ForgeDB,
  principal: UserRecord | null,
  repo: RepositoryRecord,
  action: Action,
): boolean {
  if (action === "read" && repo.visibility === "public") return true;
  if (!principal) return false;
  const level = db.getGrant(repo.id, principal.id);
  if (!level) return false;
  return RANK[level] >= RANK[action];
}
