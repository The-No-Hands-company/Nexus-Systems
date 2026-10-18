import type { AppEntry } from "../api";
import { EMPTY_QUERY_CAP as CATALOG_EMPTY_QUERY_CAP, orderedLauncherApps, searchCatalog } from "./catalog";

/** Search only apps that passed the shared launcher policy. */
export function filterApps(apps: AppEntry[], query: string): AppEntry[] {
  return searchCatalog(apps, query);
}

/** Bound on the drawer's empty-query state. Named so the "never all 86" rule
 *  reads as a constant, not a magic number scattered through the file. */
export const EMPTY_QUERY_CAP = CATALOG_EMPTY_QUERY_CAP;

/**
 * What the drawer shows before anyone types anything.
 *
 * Deliberately not `filterApps(apps, "")`: that function's empty-query branch
 * returns every eligible app, while this preserves pins, recents and a cap.
 */
export function emptyQueryApps(
  apps: AppEntry[],
  opts: { pinnedIds?: string[]; recentIds?: string[] } = {},
): AppEntry[] {
  const { pinnedIds = [], recentIds = [] } = opts;
  return orderedLauncherApps(apps, pinnedIds, recentIds);
}
