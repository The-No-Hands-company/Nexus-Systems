import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import {
  getCatalogPreferences,
  normalizeCatalogPreferences,
  saveCatalogPreferences,
} from "../src/preferences";

let directory: string;
let previousPath: string | undefined;

beforeEach(() => {
  directory = mkdtempSync(join(tmpdir(), "nexus-auth-catalog-"));
  previousPath = process.env.NEXUS_AUTH_CATALOG_PREFERENCES_PATH;
  process.env.NEXUS_AUTH_CATALOG_PREFERENCES_PATH = join(directory, "preferences.json");
});

afterEach(() => {
  if (previousPath === undefined) delete process.env.NEXUS_AUTH_CATALOG_PREFERENCES_PATH;
  else process.env.NEXUS_AUTH_CATALOG_PREFERENCES_PATH = previousPath;
  rmSync(directory, { recursive: true, force: true });
});

describe("catalog preferences", () => {
  test("defaults to the Everyday shelf and isolates users", () => {
    expect(getCatalogPreferences("alice")).toEqual({ pinnedIds: [], recentIds: [], preferredShelf: "everyday" });
    saveCatalogPreferences("alice", { pinnedIds: ["nexus-mail"], recentIds: ["nexus-chat"], preferredShelf: "work" });
    expect(getCatalogPreferences("alice")).toEqual({ pinnedIds: ["nexus-mail"], recentIds: ["nexus-chat"], preferredShelf: "work" });
    expect(getCatalogPreferences("bob")).toEqual({ pinnedIds: [], recentIds: [], preferredShelf: "everyday" });
  });

  test("normalizes invalid shelves, duplicate ids, and caps recents", () => {
    const recentIds = Array.from({ length: 12 }, (_, index) => `app-${index}`);
    expect(normalizeCatalogPreferences({
      preferredShelf: "not-a-shelf",
      pinnedIds: [" nexus-mail ", "nexus-mail", "", 4],
      recentIds,
    })).toEqual({
      preferredShelf: "everyday",
      pinnedIds: ["nexus-mail"],
      recentIds: recentIds.slice(0, 8),
    });
  });

  test("persists through a fresh read", () => {
    saveCatalogPreferences("alice", { pinnedIds: ["nexus-calendar"], recentIds: [], preferredShelf: "personal" });
    expect(getCatalogPreferences("alice").preferredShelf).toBe("personal");
  });
});
