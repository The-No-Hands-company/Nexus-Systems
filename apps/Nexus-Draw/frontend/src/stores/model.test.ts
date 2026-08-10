import { describe, it, expect } from "vitest";
import { makeElement, resolveStyleMode, type ElementData } from "./model";

describe("makeElement", () => {
  it("creates a rectangle with defaults and a stable seed", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
    expect(el.elementType).toBe("rectangle");
    expect(el.style.stroke).toBeTypeOf("string");
    expect(el.style.strokeWidth).toBeGreaterThan(0);
    expect(el.seed).toBeTypeOf("number");
    expect(el.id).toMatch(/.+/);
  });
});

describe("resolveStyleMode", () => {
  it("uses the element override when set", () => {
    const el = makeElement("rectangle", {}, { styleMode: "sketch" });
    expect(resolveStyleMode(el, "clean")).toBe("sketch");
  });
  it("falls back to the board default when unset", () => {
    const el = makeElement("rectangle", {});
    delete el.style.styleMode;
    expect(resolveStyleMode(el, "sketch")).toBe("sketch");
  });
});