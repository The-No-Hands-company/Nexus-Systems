import { useCallback, useEffect, useState } from "react";

const KEY = "nexus.pinnedApps";

/**
 * Shell-local only, same footing as density (see useDensity.ts): a preference
 * that lives in this origin's localStorage and does not travel cross-origin.
 * User-chosen pins, not derived from usage — the Pinned widget falls back to
 * healthy apps when this is empty, it does not invent pins on its own.
 */
export function readPinnedIds(): string[] {
  if (typeof localStorage === "undefined") return [];
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return [];
    const parsed: unknown = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed.filter((x): x is string => typeof x === "string") : [];
  } catch {
    // A hand-edited or corrupted value degrades to "no pins", not a crash.
    return [];
  }
}

export function writePinnedIds(ids: string[]): void {
  localStorage.setItem(KEY, JSON.stringify(ids));
}

export function usePinnedApps(): [string[], (id: string) => void] {
  const [ids, setIds] = useState<string[]>(readPinnedIds);

  // Reads happen synchronously from state; this just keeps other tabs/consumers
  // that re-mount in sync with what is on disk.
  useEffect(() => { setIds(readPinnedIds()); }, []);

  const toggle = useCallback((id: string) => {
    setIds((current) => {
      const next = current.includes(id)
        ? current.filter((x) => x !== id)
        : [...current, id];
      writePinnedIds(next);
      return next;
    });
  }, []);

  return [ids, toggle];
}
