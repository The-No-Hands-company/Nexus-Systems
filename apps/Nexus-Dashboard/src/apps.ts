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
  /** Where the app actually lives: an absolute origin to frame, or the same
   *  relative path as `path` when the shell renders it itself. */
  url: string;
  /** The in-shell route this app is reached at — /chat, /mail, /cloud.
   *
   *  The path names the app, never how the shell delivers it. Whether Chat is
   *  framed and Mail is rendered natively is an implementation detail that can
   *  change; if the URL encoded it, moving Mail to its own origin would break
   *  every link to it even though the app did not move. */
  path: string;
  health: "healthy" | "offline";
};

/**
 * Paths the shell owns. An app whose id would land on one of these keeps its
 * `/a/<id>` route instead, because a registered app called "account" must not
 * be able to take over the account page.
 */
const RESERVED = new Set(["account", "admin", "request", "claim", "a", "api", "health", ""]);

/** `nexus-chat` -> `/chat`, falling back to `/a/<id>` on a reserved collision. */
export function pathForApp(id: string): string {
  const slug = id.replace(/^nexus-/, "").toLowerCase();
  return RESERVED.has(slug) ? `/a/${id}` : `/${slug}`;
}

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
export function toAppEntries(
  payload: unknown,
  authHost: string,
  selfHost?: string,
  cloudHost?: string,
): AppEntry[] {
  const tools = (payload as { tools?: unknown } | null)?.tools;
  if (!Array.isArray(tools)) return [];

  const entries: AppEntry[] = [];
  for (const raw of tools as RawTool[]) {
    if (!raw || typeof raw !== "object") continue;

    const url = str(raw.publicUrl);
    const id = str(raw.id);
    if (!url || !id) continue;
    // The identity service is not an app you open, and neither is this page:
    // a tile linking to the dashboard, shown on the dashboard, is a button
    // that goes where you already are.
    if (url.includes(authHost)) continue;
    if (selfHost && url.includes(selfHost)) continue;
    // One tile per destination. Several tools can legitimately share a public
    // address — a published site and the backend behind it are two records
    // pointing at one host — and the person looking at the grid does not care
    // which internal record won, only that the app appears once.
    if (entries.some((e) => e.url === url)) continue;

    // Cloud's console is a shell-native view now (/cloud, /cloud/tools, ...),
    // not a site the shell frames. A relative path here — rather than
    // https://cloud.<domain> — is how the Launcher and the home grid know to
    // route in-app instead of opening/framing an external host. See
    // docs/superpowers/specs/2026-08-14-cloud-console-as-shell-views-design.md.
    const isCloud = !!cloudHost && url.includes(cloudHost);

    entries.push({
      id,
      name: str(raw.name) || id,
      description: str(raw.description),
      url: isCloud ? "/cloud" : url,
      path: isCloud ? "/cloud" : pathForApp(id),
      // Anything that is not explicitly healthy is treated as offline, so a
      // missing or unexpected value fails safe: the tile renders unlinked
      // rather than inviting a click that goes nowhere.
      health: raw.health === "healthy" ? "healthy" : "offline",
    });
  }

  return entries.sort((a, b) => a.name.localeCompare(b.name));
}

/**
 * Views this shell serves itself, which therefore never appear in Cloud's
 * registry no matter how healthy they are.
 *
 * Mail is the case that exposed the gap. Unlike Chat or Draw it has no public
 * host of its own — the webmail UI is part of this app and talks to the mail
 * API over a private proxy — so there is nothing for Cloud to register and
 * nothing to frame. Registering a record pointing at `app.<domain>/mail` would
 * be worse than the bug: toAppEntries drops `selfHost` on purpose, so the
 * record would be discarded, and if it were not, the shell would frame itself.
 *
 * The grid is still data — it is data from two sources, the registry and the
 * shell, rather than a hardcoded list of apps.
 */
export function shellNativeEntries(opts: { mailHealthy: boolean }): AppEntry[] {
  return [
    {
      id: "nexus-email",
      name: "Nexus Mail",
      description: "Sovereign mail — read, compose and search your Nexus mailbox",
      url: "/mail",
      path: "/mail",
      health: opts.mailHealthy ? "healthy" : "offline",
    },
  ];
}

/**
 * Merges the shell's own views into the registry grid.
 *
 * A shell-native view wins over a registry record with the same id or the same
 * destination: if Nexus-Email is ever registered with Cloud as well, the
 * in-shell /mail route is the one that works, and two tiles for one mailbox
 * would be a puzzle rather than a feature.
 */
export function mergeApps(registry: AppEntry[], native: AppEntry[]): AppEntry[] {
  const kept = registry.filter(
    (r) => !native.some((n) => n.id === r.id || n.url === r.url || n.path === r.path),
  );
  return [...native, ...kept].sort((a, b) => a.name.localeCompare(b.name));
}
