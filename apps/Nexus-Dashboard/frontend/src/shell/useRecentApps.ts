import { useCallback, useState } from "react";

const KEY = "nexus.recentApps";
export const MAX_RECENT_APPS = 8;

export function readRecentIds(): string[] {
  if (typeof localStorage === "undefined") return [];
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return [];
    const parsed: unknown = JSON.parse(raw);
    return Array.isArray(parsed)
      ? parsed.filter((item): item is string => typeof item === "string").slice(0, MAX_RECENT_APPS)
      : [];
  } catch {
    return [];
  }
}

export function writeRecentIds(ids: string[]): void {
  if (typeof localStorage === "undefined") return;
  const unique = ids.filter((id, index) => typeof id === "string" && ids.indexOf(id) === index);
  localStorage.setItem(KEY, JSON.stringify(unique.slice(0, MAX_RECENT_APPS)));
}

export function recordRecentId(id: string): void {
  if (!id) return;
  writeRecentIds([id, ...readRecentIds().filter((existing) => existing !== id)]);
}

export function useRecentApps(): [string[], (id: string) => void] {
  const [ids, setIds] = useState<string[]>(readRecentIds);
  const record = useCallback((id: string) => {
    setIds((current) => {
      const next = [id, ...current.filter((existing) => existing !== id)].slice(0, MAX_RECENT_APPS);
      writeRecentIds(next);
      return next;
    });
  }, []);
  return [ids, record];
}
