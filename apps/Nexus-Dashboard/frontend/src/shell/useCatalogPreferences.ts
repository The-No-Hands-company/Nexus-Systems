import { useCallback, useEffect, useState } from "react";
import {
  getCatalogPreferences,
  saveCatalogPreferences,
  type CatalogPreferences,
  type CatalogShelf,
} from "../api";

const PINNED_KEY = "nexus.pinnedApps";
const RECENT_KEY = "nexus.recentApps";
const SHELF_KEY = "nexus.preferredShelf";
const MAX_RECENT = 8;
const DEFAULT_PREFERENCES: CatalogPreferences = {
  pinnedIds: [],
  recentIds: [],
  preferredShelf: "everyday",
};

function readIds(key: string): string[] {
  if (typeof localStorage === "undefined") return [];
  try {
    const value: unknown = JSON.parse(localStorage.getItem(key) ?? "null");
    if (!Array.isArray(value)) return [];
    return [...new Set(value.filter((item): item is string => typeof item === "string" && item.trim() !== ""))];
  } catch {
    return [];
  }
}

function readLocalPreferences(): CatalogPreferences {
  const shelf = typeof localStorage === "undefined" ? "everyday" : localStorage.getItem(SHELF_KEY);
  const allowed: CatalogShelf[] = ["everyday", "work", "create", "develop", "operate", "personal", "explore"];
  return {
    pinnedIds: readIds(PINNED_KEY),
    recentIds: readIds(RECENT_KEY).slice(0, MAX_RECENT),
    preferredShelf: allowed.includes(shelf as CatalogShelf) ? shelf as CatalogShelf : "everyday",
  };
}

function writeLocalPreferences(preferences: CatalogPreferences): void {
  if (typeof localStorage === "undefined") return;
  localStorage.setItem(PINNED_KEY, JSON.stringify(preferences.pinnedIds));
  localStorage.setItem(RECENT_KEY, JSON.stringify(preferences.recentIds.slice(0, MAX_RECENT)));
  localStorage.setItem(SHELF_KEY, preferences.preferredShelf);
}

function mergeMigration(remote: CatalogPreferences, local: CatalogPreferences): { value: CatalogPreferences; migrated: boolean } {
  // A new account has no server preferences yet. Preserve the existing shell
  // choices once, then immediately write the merged record to Auth.
  if (remote.pinnedIds.length || remote.recentIds.length || remote.preferredShelf !== "everyday") return { value: remote, migrated: false };
  const value = {
    pinnedIds: local.pinnedIds,
    recentIds: local.recentIds.slice(0, MAX_RECENT),
    preferredShelf: local.preferredShelf,
  };
  return { value, migrated: local.pinnedIds.length > 0 || local.recentIds.length > 0 || local.preferredShelf !== "everyday" };
}

/**
 * Account-backed catalog preferences with a local-storage migration path.
 * Auth is authoritative after the first successful read; local storage keeps
 * the launcher usable during an old-server or offline deployment.
 */
export function useCatalogPreferences() {
  const [preferences, setPreferences] = useState<CatalogPreferences>(readLocalPreferences);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    let cancelled = false;
    const local = readLocalPreferences();
    void getCatalogPreferences().then((remote) => {
      if (cancelled) return;
      const { value: merged, migrated } = mergeMigration(remote, local);
      setPreferences(merged);
      writeLocalPreferences(merged);
      if (migrated) void saveCatalogPreferences(merged).catch(() => {});
    }).catch(() => {
      if (!cancelled) setPreferences(local);
    }).finally(() => {
      if (!cancelled) setLoading(false);
    });
    return () => { cancelled = true; };
  }, []);

  const update = useCallback((next: CatalogPreferences) => {
    const normalized: CatalogPreferences = {
      pinnedIds: [...new Set(next.pinnedIds)],
      recentIds: [...new Set(next.recentIds)].slice(0, MAX_RECENT),
      preferredShelf: next.preferredShelf,
    };
    setPreferences(normalized);
    writeLocalPreferences(normalized);
    void saveCatalogPreferences(normalized).catch(() => {});
  }, []);

  const togglePinned = useCallback((id: string) => {
    update({
      ...preferences,
      pinnedIds: preferences.pinnedIds.includes(id)
        ? preferences.pinnedIds.filter((item) => item !== id)
        : [...preferences.pinnedIds, id],
    });
  }, [preferences, update]);

  const recordRecent = useCallback((id: string) => {
    update({ ...preferences, recentIds: [id, ...preferences.recentIds.filter((item) => item !== id)] });
  }, [preferences, update]);

  const setPreferredShelf = useCallback((preferredShelf: CatalogShelf) => {
    update({ ...preferences, preferredShelf });
  }, [preferences, update]);

  return { preferences, loading, setPreferences: update, togglePinned, recordRecent, setPreferredShelf };
}

export function localCatalogPreferencesForTests(): CatalogPreferences {
  return readLocalPreferences();
}

export { DEFAULT_PREFERENCES };
