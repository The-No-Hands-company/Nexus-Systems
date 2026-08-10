import { describe, it, expect } from "vitest";
import { tools } from "./Toolbar";

describe("toolbar shortcuts", () => {
  // "ellipse" and "eraser" both used to advertise "E". The canvas bound "e" to
  // ellipse, so the eraser's badge and tooltip promised a key that did nothing.
  // A collision is invisible in the UI, so assert the defining property.
  it("gives every tool a distinct hotkey", () => {
    const keys = tools.map((t) => t.key);
    expect(new Set(keys).size).toBe(keys.length);
  });

  it("exposes only tools the editor actually implements", () => {
    // "fill" and "zoom" were selectable but had no implementation behind them.
    const ids = tools.map((t) => t.id);
    expect(ids).not.toContain("fill");
    expect(ids).not.toContain("zoom");
  });
});
