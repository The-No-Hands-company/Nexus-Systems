import { describe, expect, test } from "bun:test";
import { flattenTokens } from "../src/flatten";

describe("flattenTokens", () => {
  test("joins nested keys with dashes and prefixes with --nexus", () => {
    const out = flattenTokens({ color: { bg: { canvas: "#090d0d" } } });
    expect(out).toEqual([{ name: "--nexus-color-bg-canvas", value: "#090d0d" }]);
  });

  test("stringifies numbers, because CSS custom properties are text", () => {
    const out = flattenTokens({ space: { "4": 16 } });
    expect(out).toEqual([{ name: "--nexus-space-4", value: "16" }]);
  });

  test("keeps every leaf, not just the first", () => {
    const out = flattenTokens({ radius: { sm: 6, md: 10, pill: 999 } });
    expect(out.map((t) => t.name)).toEqual([
      "--nexus-radius-sm",
      "--nexus-radius-md",
      "--nexus-radius-pill",
    ]);
  });

  test("preserves values that are already CSS, like rgba and cubic-bezier", () => {
    const out = flattenTokens({
      border: { subtle: "rgba(230,242,238,0.12)" },
      motion: { easing: { standard: "cubic-bezier(0.2,0,0,1)" } },
    });
    expect(out).toContainEqual({ name: "--nexus-border-subtle", value: "rgba(230,242,238,0.12)" });
    expect(out).toContainEqual({
      name: "--nexus-motion-easing-standard",
      value: "cubic-bezier(0.2,0,0,1)",
    });
  });

  test("skips metadata keys that are not design values", () => {
    // $schema and version describe the file, not how anything looks.
    const out = flattenTokens({ $schema: "https://x", version: "1.0.0", radius: { sm: 6 } });
    expect(out).toEqual([{ name: "--nexus-radius-sm", value: "6" }]);
  });

  test("returns nothing for an empty object rather than throwing", () => {
    expect(flattenTokens({})).toEqual([]);
  });
});
