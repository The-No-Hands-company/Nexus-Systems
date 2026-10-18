import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

export const CATALOG_SHELVES = [
  "everyday",
  "work",
  "create",
  "develop",
  "operate",
  "personal",
  "explore",
] as const;
export type CatalogShelf = (typeof CATALOG_SHELVES)[number];

export type CatalogPreferences = {
  pinnedIds: string[];
  recentIds: string[];
  preferredShelf: CatalogShelf;
};

const DEFAULT_PREFERENCES: CatalogPreferences = {
  pinnedIds: [],
  recentIds: [],
  preferredShelf: "everyday",
};
const MAX_RECENT_IDS = 8;
const MAX_PINNED_IDS = 100;
const MAX_ID_LENGTH = 160;

function preferencesPath(): string {
  return process.env.NEXUS_AUTH_CATALOG_PREFERENCES_PATH?.trim() ||
    join(process.cwd(), "data", "catalog-preferences.json");
}

function cloneDefault(): CatalogPreferences {
  return { pinnedIds: [], recentIds: [], preferredShelf: DEFAULT_PREFERENCES.preferredShelf };
}

function validId(value: unknown): value is string {
  return typeof value === "string" && value.trim().length > 0 && value.length <= MAX_ID_LENGTH;
}

function ids(value: unknown, max: number): string[] {
  if (!Array.isArray(value)) return [];
  const seen = new Set<string>();
  const result: string[] = [];
  for (const item of value) {
    if (!validId(item)) continue;
    const id = item.trim();
    if (seen.has(id)) continue;
    seen.add(id);
    result.push(id);
    if (result.length >= max) break;
  }
  return result;
}

export function normalizeCatalogPreferences(value: unknown): CatalogPreferences {
  const record = typeof value === "object" && value !== null ? value as Record<string, unknown> : {};
  const preferredShelf = typeof record.preferredShelf === "string" &&
    CATALOG_SHELVES.includes(record.preferredShelf as CatalogShelf)
    ? record.preferredShelf as CatalogShelf
    : DEFAULT_PREFERENCES.preferredShelf;
  return {
    pinnedIds: ids(record.pinnedIds, MAX_PINNED_IDS),
    recentIds: ids(record.recentIds, MAX_RECENT_IDS),
    preferredShelf,
  };
}

function readState(): Record<string, unknown> {
  const path = preferencesPath();
  if (!existsSync(path)) return {};
  try {
    const parsed: unknown = JSON.parse(readFileSync(path, "utf8"));
    return typeof parsed === "object" && parsed !== null && !Array.isArray(parsed)
      ? parsed as Record<string, unknown>
      : {};
  } catch {
    return {};
  }
}

function writeState(state: Record<string, unknown>): void {
  const path = preferencesPath();
  mkdirSync(dirname(path), { recursive: true });
  const temp = `${path}.${process.pid}.${Date.now()}.tmp`;
  writeFileSync(temp, `${JSON.stringify(state, null, 2)}\n`, { mode: 0o600 });
  renameSync(temp, path);
}

/** Account-scoped catalog preferences. The caller supplies only its own id. */
export function getCatalogPreferences(userId: string): CatalogPreferences {
  const state = readState();
  return normalizeCatalogPreferences(state[userId] ?? DEFAULT_PREFERENCES);
}

export function saveCatalogPreferences(userId: string, value: unknown): CatalogPreferences {
  if (!userId.trim()) throw new Error("user id is required");
  const next = normalizeCatalogPreferences(value);
  const state = readState();
  state[userId] = next;
  writeState(state);
  return next;
}

export function resetCatalogPreferencesForTests(): void {
  const path = preferencesPath();
  if (existsSync(path)) {
    writeState({});
  }
}

export const catalogPreferencesDefaults = cloneDefault();
