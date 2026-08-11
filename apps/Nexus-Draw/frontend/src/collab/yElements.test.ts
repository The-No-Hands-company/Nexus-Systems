import { describe, it, expect } from "vitest";
import * as Y from "yjs";
import { yToElements, writeElements, createElementDoc, elementsEqual } from "./yElements";
import type { ElementData, ElementType, ElementStyle } from "../stores/model";

const DEFAULT_STYLE: ElementStyle = {
  stroke: "#e4e4e7", fill: "none", strokeWidth: 2, strokeStyle: "solid",
  opacity: 1, radius: 8, fontFamily: "ui-sans-serif, system-ui", fontSize: 20, textAlign: "left",
};

const makeTestElement = (id: string, type: ElementType, data: Record<string, any>): ElementData => ({
  id,
  elementType: type,
  data,
  style: { ...DEFAULT_STYLE },
  transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
  order: 0,
  seed: 12345,
});

const rect = (id: string, x: number) => makeTestElement(id, "rectangle", { x, y: 0, width: 10, height: 10 });

describe("yElements", () => {
  it("writeElements → yToElements round-trips in order", () => {
    const doc = createElementDoc([]);
    const els = [rect("a", 0), rect("b", 5), rect("c", 10)];
    writeElements(doc, els);
    const out = yToElements(doc);
    expect(out.map((e) => e.id)).toEqual(["a", "b", "c"]);
    expect(out[0].data.x).toBe(0);
  });

  it("writeElements removes stale entries and reorders", () => {
    const doc = createElementDoc([]);
    writeElements(doc, [rect("a", 0), rect("b", 5)]);
    writeElements(doc, [rect("b", 5)]); // remove 'a'
    expect(yToElements(doc).map((e) => e.id)).toEqual(["b"]);
  });

  it("reconciles diffs without clobbering remote-only ids", () => {
    const doc = createElementDoc([]);
    writeElements(doc, [rect("a", 0), rect("b", 5)]);
    // Simulate a remote peer adding 'remote' and touching 'a' between our writes:
    doc.transact(() => { doc.getMap("elements").set("remote", rect("remote", 99)); });
    writeElements(doc, [rect("a", 1), rect("b", 5)]);
    const out = yToElements(doc);
    expect(out.some((e) => e.id === "remote")).toBe(true);
    expect(out.find((e) => e.id === "a")!.data.x).toBe(1);
  });

  it("elementsEqual detects ordering + content changes", () => {
    expect(elementsEqual([rect("a", 0)], [rect("a", 0)])).toBe(true);
    expect(elementsEqual([rect("a", 0)], [rect("a", 1)])).toBe(false);
    expect(elementsEqual([rect("a", 0), rect("b", 1)], [rect("b", 1), rect("a", 0)])).toBe(false);
  });
});