import { describe, expect, it } from "bun:test";
import { buildCatalogReleaseFixture } from "../../Nexus-Cloud/src/catalog/release-fixture";
import {
  groupAppsByShelf,
  normalizeCatalogEntry,
  searchCatalog,
} from "../frontend/src/shell/catalog";
import { toAppEntries } from "../src/apps";

describe("catalog release contract", () => {
  it("keeps approved placement, Explore fallback, and offline visibility intact", () => {
    const entries = toAppEntries(buildCatalogReleaseFixture(), "auth.tnhc.dev").map(
      normalizeCatalogEntry,
    );
    const shelves = groupAppsByShelf(entries);

    expect(shelves.everyday.map((app) => app.id)).toContain("nexus-calendar");
    expect(shelves.create.map((app) => app.id)).toEqual(
      expect.arrayContaining(["nexus-design", "nexus-video"]),
    );
    expect(shelves.develop.map((app) => app.id)).toContain("nexus-forge");
    expect(shelves.operate.map((app) => app.id)).toContain("nexus-cloud");
    expect(shelves.explore.map((app) => app.id)).toEqual(
      expect.arrayContaining(["nexus-experimental", "legacy-notes"]),
    );
    expect(entries.find((app) => app.id === "nexus-video")).toMatchObject({ health: "offline" });
    expect(searchCatalog(entries, "video")).toEqual(
      expect.arrayContaining([expect.objectContaining({ id: "nexus-video" })]),
    );
  });
});
