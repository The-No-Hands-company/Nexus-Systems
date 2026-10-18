import type { AppEntry } from "../api";
import { EMPTY_QUERY_CAP as CATALOG_EMPTY_QUERY_CAP, orderedLauncherApps } from "./catalog";

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
    ? apps.filter((a) => a.visibility !== "hidden" && [a.name, a.id, a.description, ...(a.capabilities ?? [])].join(" ").toLowerCase().includes(q))
    : apps.filter((a) => a.visibility !== "hidden");

  return matched.sort((a, b) => {
    if (a.health !== b.health) return a.health === "healthy" ? -1 : 1;
    return a.name.localeCompare(b.name);
  });
}

/** Bound on the drawer's empty-query state. Named so the "never all 86" rule
 *  reads as a constant, not a magic number scattered through the file. */
export const EMPTY_QUERY_CAP = CATALOG_EMPTY_QUERY_CAP;

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
  return orderedLauncherApps(apps, pinnedIds, recentIds);
}
