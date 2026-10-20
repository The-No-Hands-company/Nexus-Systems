import type { AppEntry, CatalogReleaseState } from "../api";

export const CATALOG_SHELVES = ["everyday", "work", "create", "develop", "operate", "personal", "explore"] as const;
export type CatalogShelf = typeof CATALOG_SHELVES[number];
export const CATALOG_AUDIENCES = ["general", "business", "creator", "developer", "operations", "power-user"] as const;
export type CatalogAudience = typeof CATALOG_AUDIENCES[number];
export type CatalogMaturity = "stable" | "preview" | "experimental";
export type CatalogVisibility = "default" | "discoverable" | "restricted" | "hidden";
export type CatalogReviewStatus = "approved" | "pending" | "deferred" | "rejected";
export type CatalogSurface = "user-app" | "admin-console" | "platform-service" | "developer-tool";

const isOneOf = <T extends readonly string[]>(values: T, value: unknown): value is T[number] =>
  typeof value === "string" && values.includes(value);

const stringArray = (value: unknown): string[] =>
  Array.isArray(value) && value.every((item) => typeof item === "string") ? [...value] as string[] : [];

export function normalizeCatalogEntry(raw: unknown): AppEntry {
  const source = raw && typeof raw === "object" ? raw as Record<string, unknown> : {};
  const id = typeof source.id === "string" ? source.id : "unknown-app";
  const name = typeof source.name === "string" ? source.name : id;
  const path = typeof source.path === "string" ? source.path : "";
  const shelf = isOneOf(CATALOG_SHELVES, source.primaryShelf) ? source.primaryShelf : "explore";
  const secondaryShelves = stringArray(source.secondaryShelves).filter((value): value is CatalogShelf => isOneOf(CATALOG_SHELVES, value));
  const audiences = stringArray(source.audiences).filter((value): value is CatalogAudience => isOneOf(CATALOG_AUDIENCES, value));
  const maturity: CatalogMaturity = source.maturity === "stable" || source.maturity === "preview" ? source.maturity : "experimental";
  const visibility: CatalogVisibility = source.visibility === "default" || source.visibility === "discoverable" || source.visibility === "restricted" || source.visibility === "hidden" ? source.visibility : "hidden";
  const reviewStatus: CatalogReviewStatus = source.reviewStatus === "approved" || source.reviewStatus === "deferred" || source.reviewStatus === "rejected" ? source.reviewStatus : "pending";
  // Missing wire metadata cannot turn a legacy record into a released app.
  const inferredSurface: CatalogSurface = "platform-service";
  const surface: CatalogSurface = source.surface === "admin-console" || source.surface === "platform-service" || source.surface === "developer-tool" || source.surface === "user-app"
    ? source.surface
    : inferredSurface;
  const releaseState: CatalogReleaseState = source.releaseState === "released" || source.releaseState === "retired"
    ? source.releaseState
    : "staged";
  const delivery = source.delivery === "shell-native" || source.delivery === "proxied-app" || source.delivery === "framed" || source.delivery === "external" ? source.delivery : "framed";
  return {
    id,
    name,
    description: typeof source.description === "string" ? source.description : "",
    path,
    ...(typeof source.publicUrl === "string" ? { publicUrl: source.publicUrl } : {}),
    delivery,
    health: source.health === "healthy" ? "healthy" : "offline",
    primaryShelf: shelf,
    secondaryShelves,
    audiences,
    capabilities: stringArray(source.capabilities),
    maturity,
    visibility,
    reviewStatus,
    surface,
    releaseState,
  };
}

function safePath(path: string): boolean {
  return /^\/[A-Za-z0-9._~-]+(?:\/[A-Za-z0-9._~-]+)*$/.test(path) &&
    path.split("/").slice(1).every((segment) => segment !== "." && segment !== "..");
}

function safePublicUrl(value: string): boolean {
  if (!value || value.trim() !== value || /[\u0000-\u001f\u007f]/.test(value)) return false;
  try {
    const url = new URL(value);
    return (url.protocol === "http:" || url.protocol === "https:") &&
      !!url.hostname && !url.username && !url.password;
  } catch {
    return false;
  }
}

/** The same fail-closed projection used by every ordinary app surface. */
export function isLauncherApp(app: AppEntry): boolean {
  const destinationValid = app.publicUrl === undefined
    ? app.delivery === "shell-native"
    : typeof app.publicUrl === "string" && safePublicUrl(app.publicUrl);
  return app.surface === "user-app" &&
    (app.releaseState === "released" || app.releaseState === "staged") &&
    app.maturity === "stable" && app.health === "healthy" &&
    app.reviewStatus === "approved" &&
    (app.visibility === "default" || app.visibility === "discoverable") &&
    safePath(app.path) && destinationValid;
}

const stableName = (a: AppEntry, b: AppEntry) => a.name.localeCompare(b.name) || a.id.localeCompare(b.id);

export function appsForShelf(apps: AppEntry[], shelf: CatalogShelf): AppEntry[] {
  return apps.filter((app) => isLauncherApp(app) && (app.primaryShelf === shelf || (app.secondaryShelves ?? []).includes(shelf))).sort(stableName);
}

export function groupAppsByShelf(apps: AppEntry[]): Record<CatalogShelf, AppEntry[]> {
  return Object.fromEntries(CATALOG_SHELVES.map((shelf) => [shelf, appsForShelf(apps, shelf)])) as Record<CatalogShelf, AppEntry[]>;
}

export function searchCatalog(
  apps: AppEntry[],
  query: string,
  filters: { shelf?: CatalogShelf | "all"; audience?: CatalogAudience | "all"; capability?: string | "all" } = {},
): AppEntry[] {
  const needle = query.trim().toLowerCase();
  return apps
    .filter(isLauncherApp)
    .filter((app) => !needle || [app.name, app.id, app.description, ...(app.capabilities ?? [])].join(" ").toLowerCase().includes(needle))
    .filter((app) => !filters.shelf || filters.shelf === "all" || app.primaryShelf === filters.shelf || (app.secondaryShelves ?? []).includes(filters.shelf))
    .filter((app) => !filters.audience || filters.audience === "all" || (app.audiences ?? []).includes(filters.audience))
    .filter((app) => !filters.capability || filters.capability === "all" || (app.capabilities ?? []).includes(filters.capability))
    .sort(stableName);
}

export const EMPTY_QUERY_CAP = 12;

export function orderedLauncherApps(apps: AppEntry[], pinnedIds: string[] = [], recentIds: string[] = []): AppEntry[] {
  const byId = new Map(apps.filter(isLauncherApp).map((app) => [app.id, app]));
  const ordered: AppEntry[] = [];
  const add = (id: string) => {
    const app = byId.get(id);
    if (app && !ordered.includes(app)) ordered.push(app);
  };
  pinnedIds.forEach(add);
  recentIds.forEach(add);
  [...byId.values()].sort(stableName).forEach((app) => { if (!ordered.includes(app)) ordered.push(app); });
  return ordered.slice(0, EMPTY_QUERY_CAP);
}
