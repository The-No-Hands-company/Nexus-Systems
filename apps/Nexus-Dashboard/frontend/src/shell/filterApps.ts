import type { AppEntry } from "../api";

/**
 * Healthy first, then offline, alphabetical within each group.
 *
 * Offline entries are returned rather than dropped: the drawer shows them
 * dimmed and unactivatable, because hiding them would mean the shell silently
 * denies that half the ecosystem exists.
 */
export function filterApps(apps: AppEntry[], query: string): AppEntry[] {
  const q = query.trim().toLowerCase();
  const matched = q
    ? apps.filter((a) => a.name.toLowerCase().includes(q) || a.id.toLowerCase().includes(q))
    : [...apps];

  return matched.sort((a, b) => {
    if (a.health !== b.health) return a.health === "healthy" ? -1 : 1;
    return a.name.localeCompare(b.name);
  });
}

/** Bound on the drawer's empty-query state. Named so the "never all 86" rule
 *  reads as a constant, not a magic number scattered through the file. */
export const EMPTY_QUERY_CAP = 12;

/**
 * What the drawer shows before anyone types anything.
 *
 * Deliberately not `filterApps(apps, "")`: that function's empty-query branch
 * returns *everything* (a legitimate behaviour for "search with no filter"),
 * and reusing it here would reintroduce the exact wall — all 86 apps, more
 * than half offline — that moving the directory into a drawer exists to
 * remove. This is a separate, testable policy: pinned first, then recently
 * used, then the rest of the healthy apps, alphabetical, capped at
 * EMPTY_QUERY_CAP. Offline apps never appear here — typing is how you reach
 * them, deliberately, per the spec.
 */
export function emptyQueryApps(
  apps: AppEntry[],
  opts: { pinnedIds?: string[]; recentIds?: string[] } = {},
): AppEntry[] {
  const { pinnedIds = [], recentIds = [] } = opts;
  const byId = new Map(apps.map((a) => [a.id, a]));
  const seen = new Set<string>();
  const ordered: AppEntry[] = [];

  function take(id: string) {
    const app = byId.get(id);
    if (!app || app.health !== "healthy" || seen.has(id)) return;
    seen.add(id);
    ordered.push(app);
  }

  for (const id of pinnedIds) take(id);
  for (const id of recentIds) take(id);
  for (const a of [...apps].sort((x, y) => x.name.localeCompare(y.name))) {
    take(a.id);
  }

  return ordered.slice(0, EMPTY_QUERY_CAP);
}
