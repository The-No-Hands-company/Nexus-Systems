import { describe, expect, it } from "vitest";
import {
  buildHierarchyRows,
  buildUniqueObjectName,
  collectDescendantIds,
  filterHierarchyItems,
} from "../src/engine/editor-scene-utils";

describe("filterHierarchyItems", () => {
  it("returns all items when the query is empty", () => {
    const items = [{ name: "Player" }, { name: "Enemy_01" }];
    expect(filterHierarchyItems(items, "")).toEqual(items);
  });

  it("filters items case-insensitively", () => {
    const items = [{ name: "Player" }, { name: "Enemy_01" }, { name: "Tree" }];
    expect(filterHierarchyItems(items, "enemy")).toEqual([{ name: "Enemy_01" }]);
  });
});

describe("buildUniqueObjectName", () => {
  it("uses the requested name when it is not taken", () => {
    expect(buildUniqueObjectName("Crate", ["Player", "Enemy"])).toBe("Crate");
  });

  it("adds a numeric suffix when the name already exists", () => {
    expect(buildUniqueObjectName("Crate", ["Crate", "Crate_2", "Crate_3"])).toBe("Crate_4");
  });

  it("falls back to Object for blank names", () => {
    expect(buildUniqueObjectName("   ", ["Object"])).toBe("Object_2");
  });
});

describe("buildHierarchyRows", () => {
  const items = [
    { id: 1, name: "RootFolder", parentId: null },
    { id: 2, name: "Player", parentId: 1 },
    { id: 3, name: "Weapon", parentId: 2 },
    { id: 4, name: "Enemy", parentId: null },
  ];

  it("returns rows in hierarchy order with depth", () => {
    expect(buildHierarchyRows(items)).toEqual([
      { item: items[3], depth: 0 },
      { item: items[0], depth: 0 },
      { item: items[1], depth: 1 },
      { item: items[2], depth: 2 },
    ]);
  });

  it("keeps matching ancestors visible during search", () => {
    expect(buildHierarchyRows(items, "weapon")).toEqual([
      { item: items[0], depth: 0 },
      { item: items[1], depth: 1 },
      { item: items[2], depth: 2 },
    ]);
  });

  it("hides collapsed descendants when no search query is active", () => {
    expect(buildHierarchyRows(items, "", new Set([1]))).toEqual([
      { item: items[3], depth: 0 },
      { item: items[0], depth: 0 },
    ]);
  });
});

describe("collectDescendantIds", () => {
  it("collects all descendant ids for a tree node", () => {
    const items = [
      { id: 1, name: "Root", parentId: null },
      { id: 2, name: "ChildA", parentId: 1 },
      { id: 3, name: "ChildB", parentId: 1 },
      { id: 4, name: "Grandchild", parentId: 2 },
    ];

    expect(Array.from(collectDescendantIds(items, 1)).sort((a, b) => a - b)).toEqual([2, 3, 4]);
  });
});
