import { beforeEach, describe, expect, it } from "vitest";
import { readRecentIds, recordRecentId, writeRecentIds } from "../useRecentApps";

describe("recent app storage", () => {
  beforeEach(() => {
    const values = new Map<string, string>();
    Object.defineProperty(globalThis, "localStorage", {
      configurable: true,
      value: {
        getItem: (key: string) => values.get(key) ?? null,
        setItem: (key: string, value: string) => values.set(key, value),
        removeItem: (key: string) => values.delete(key),
        clear: () => values.clear(),
      },
    });
  });

  it("keeps the newest app first and promotes duplicates", () => {
    writeRecentIds(["a", "b"]);
    recordRecentId("b");
    recordRecentId("c");
    expect(readRecentIds()).toEqual(["c", "b", "a"]);
  });

  it("bounds recents and ignores malformed storage", () => {
    writeRecentIds(Array.from({ length: 20 }, (_, i) => String(i)));
    expect(readRecentIds()).toHaveLength(8);
    localStorage.setItem("nexus.recentApps", "not-json");
    expect(readRecentIds()).toEqual([]);
  });
});
