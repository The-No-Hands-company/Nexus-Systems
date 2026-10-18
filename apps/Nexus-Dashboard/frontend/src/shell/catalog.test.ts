import { describe, expect, it } from "vitest";
import {
  appsForShelf,
  groupAppsByShelf,
  normalizeCatalogEntry,
  orderedLauncherApps,
  searchCatalog,
  type CatalogShelf,
} from "./catalog";
import type { AppEntry } from "../api";

const app = (overrides: Partial<AppEntry> = {}): AppEntry => ({
  id: "nexus-chat",
  name: "Nexus Chat",
  description: "Private messaging and communities",
  path: "/chat",
  publicUrl: "https://chat.tnhc.dev",
  delivery: "framed",
  health: "healthy",
  primaryShelf: "everyday",
  secondaryShelves: [],
  audiences: ["general"],
  capabilities: ["chat", "messaging"],
  maturity: "stable",
  visibility: "default",
  reviewStatus: "approved",
  surface: "user-app",
  releaseState: "released",
  ...overrides,
});

describe("normalizeCatalogEntry", () => {
  it("puts old records in discoverable Explore with pending review", () => {
    expect(normalizeCatalogEntry({ id: "legacy", name: "Legacy" })).toMatchObject({
      primaryShelf: "explore",
      secondaryShelves: [],
      maturity: "experimental",
      visibility: "discoverable",
      reviewStatus: "pending",
      surface: "platform-service",
      releaseState: "staged",
    });
  });

  it("preserves explicit release metadata", () => {
    expect(normalizeCatalogEntry({ id: "nexus-chat", surface: "user-app", releaseState: "released" })).toMatchObject({
      surface: "user-app", releaseState: "released",
    });
  });

  it("rejects malformed controlled values without losing the app identity", () => {
    expect(normalizeCatalogEntry({ id: "x", name: "X", primaryShelf: "wrong", audiences: ["general", 1] })).toMatchObject({
      id: "x",
      primaryShelf: "explore",
      audiences: [],
    });
  });
});

describe("catalog grouping and search", () => {
  const apps = [
    app(),
    app({ id: "nexus-calendar", name: "Nexus Calendar", path: "/calendar", primaryShelf: "work", secondaryShelves: ["everyday"] }),
    app({ id: "nexus-draw", name: "Nexus Draw", path: "/draw", primaryShelf: "create", health: "offline", capabilities: ["canvas"] }),
    app({ id: "nexus-hidden", name: "Hidden", path: "/hidden", visibility: "hidden" }),
  ];

  it("includes a secondary placement once per shelf", () => {
    const grouped = groupAppsByShelf(apps);
    expect(grouped.everyday.map((entry) => entry.id)).toEqual(["nexus-calendar", "nexus-chat"]);
    expect(grouped.work.map((entry) => entry.id)).toEqual(["nexus-calendar"]);
    expect(grouped.create.map((entry) => entry.id)).toEqual(["nexus-draw"]);
  });

  it("filters hidden records and supports audience/capability/text search", () => {
    expect(searchCatalog(apps, "calendar", {}).map((entry) => entry.id)).toEqual(["nexus-calendar"]);
    expect(searchCatalog(apps, "", { capability: "canvas" }).map((entry) => entry.id)).toEqual(["nexus-draw"]);
    expect(searchCatalog(apps, "", { shelf: "everyday" }).map((entry) => entry.id)).toEqual(["nexus-calendar", "nexus-chat"]);
    expect(searchCatalog(apps, "hidden", {}).map((entry) => entry.id)).toEqual([]);
  });

  it("orders pins, recents, then healthy fallback and caps the result", () => {
    const many = Array.from({ length: 20 }, (_, index) => app({ id: `nexus-${index}`, name: `App ${index}` }));
    const result = orderedLauncherApps(many, ["nexus-5"], ["nexus-3", "nexus-5"]);
    expect(result.slice(0, 2).map((entry) => entry.id)).toEqual(["nexus-5", "nexus-3"]);
    expect(result).toHaveLength(12);
  });

  it("uses the controlled shelf set", () => {
    const shelves: CatalogShelf[] = ["everyday", "work", "create", "develop", "operate", "personal", "explore"];
    expect(Object.keys(groupAppsByShelf([]))).toEqual(shelves);
    expect(appsForShelf(apps, "everyday").map((entry) => entry.id)).toEqual(["nexus-calendar", "nexus-chat"]);
  });
});
