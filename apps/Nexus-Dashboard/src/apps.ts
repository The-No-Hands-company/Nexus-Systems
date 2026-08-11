/**
 * Turns Cloud's tool registry into the grid the dashboard renders.
 *
 * The grid is data, never a hardcoded list: a new app appears by registering
 * with Cloud, and a second node's apps appear with no code change here.
 */

export type AppEntry = {
  id: string;
  name: string;
  description: string;
  url: string;
  health: "healthy" | "offline";
};

type RawTool = {
  id?: unknown;
  name?: unknown;
  description?: unknown;
  publicUrl?: unknown;
  health?: unknown;
};

function str(v: unknown): string {
  return typeof v === "string" ? v : "";
}

/**
 * `authHost` is excluded deliberately. Auth is where you sign in, not an app
 * you open, and a tile leading to the login page from inside the dashboard is
 * a dead end for someone already signed in.
 *
 * Tools without a publicUrl are dropped: most of the registry is empty
 * scaffolds — 82 of 85 at the time of writing — and a tile that cannot be
 * clicked is worse than no tile.
 *
 * A malformed payload yields an empty grid rather than throwing. Cloud is a
 * separate service that can be mid-restart or mid-deploy, and that must cost
 * the user their app list, not the whole dashboard.
 */
export function toAppEntries(payload: unknown, authHost: string): AppEntry[] {
  const tools = (payload as { tools?: unknown } | null)?.tools;
  if (!Array.isArray(tools)) return [];

  const entries: AppEntry[] = [];
  for (const raw of tools as RawTool[]) {
    if (!raw || typeof raw !== "object") continue;

    const url = str(raw.publicUrl);
    const id = str(raw.id);
    if (!url || !id) continue;
    if (url.includes(authHost)) continue;

    entries.push({
      id,
      name: str(raw.name) || id,
      description: str(raw.description),
      url,
      // Anything that is not explicitly healthy is treated as offline, so a
      // missing or unexpected value fails safe: the tile renders unlinked
      // rather than inviting a click that goes nowhere.
      health: raw.health === "healthy" ? "healthy" : "offline",
    });
  }

  return entries.sort((a, b) => a.name.localeCompare(b.name));
}
