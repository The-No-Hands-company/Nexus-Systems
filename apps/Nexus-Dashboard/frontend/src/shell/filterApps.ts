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
