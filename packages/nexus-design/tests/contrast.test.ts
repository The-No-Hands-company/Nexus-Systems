import { describe, expect, test } from "bun:test";
import { parseColor, contrastRatio, validateTheme } from "../src/contrast";

describe("parseColor", () => {
  test("reads hex and rgba", () => {
    expect(parseColor("#FFFFFF")).toEqual({ r: 255, g: 255, b: 255, a: 1 });
    expect(parseColor("#030303")).toEqual({ r: 3, g: 3, b: 3, a: 1 });
    expect(parseColor("rgba(255,255,255,0.10)")).toEqual({ r: 255, g: 255, b: 255, a: 0.1 });
  });

  test("rejects anything it does not understand rather than guessing", () => {
    expect(() => parseColor("chartreuse")).toThrow(/unsupported colour/i);
  });
});

describe("contrastRatio", () => {
  test("black on white is the documented maximum", () => {
    expect(contrastRatio("#000000", "#FFFFFF")).toBeCloseTo(21, 1);
  });

  test("a colour against itself is 1", () => {
    expect(contrastRatio("#3A7BD5", "#3A7BD5")).toBeCloseTo(1, 5);
  });

  test("order does not matter", () => {
    expect(contrastRatio("#FFFFFF", "#030303")).toBeCloseTo(contrastRatio("#030303", "#FFFFFF"), 5);
  });

  test("a translucent foreground is composited over the background first", () => {
    // 10% white over near-black is a dark grey, not white — so the ratio
    // against that same near-black must be tiny, not ~20.
    expect(contrastRatio("rgba(255,255,255,0.10)", "#030303")).toBeLessThan(2);
  });
});

describe("validateTheme", () => {
  const good = {
    "bg.canvas": "#030303", "bg.surface": "#0A0A0A", "bg.elevated": "#111111",
    "text.primary": "#FFFFFF", "text.secondary": "#A0A0A0", "text.muted": "#6E6E6E",
    "accent.primary": "#CCFF00",
    "state.success": "#2AC57D", "state.warning": "#E8B24A",
    "state.danger": "#E15D5D", "state.info": "#5CA8FF",
  };

  test("passes a palette that meets AA", () => {
    expect(validateTheme("void", good).filter((f) => !f.ok)).toEqual([]);
  });

  // The gate must be able to fail. A validator that cannot reject anything is
  // the exact failure mode this repository has been burned by.
  test("rejects text that does not meet 4.5:1", () => {
    const bad = { ...good, "text.secondary": "#151515" };
    const failures = validateTheme("void", bad).filter((f) => !f.ok);
    expect(failures.length).toBeGreaterThan(0);
    expect(failures.map((f) => f.pair)).toContain("text.secondary on bg.canvas");
  });

  test("rejects a state colour that does not meet 3:1", () => {
    const bad = { ...good, "state.success": "#0B2A1C" };
    const failures = validateTheme("void", bad).filter((f) => !f.ok);
    expect(failures.map((f) => f.pair)).toContain("state.success on bg.surface");
  });

  test("reports the ratio and the requirement, not just a boolean", () => {
    const findings = validateTheme("void", good);
    const one = findings.find((f) => f.pair === "text.primary on bg.canvas")!;
    expect(one.required).toBe(4.5);
    expect(one.ratio).toBeGreaterThan(4.5);
    expect(one.themeId).toBe("void");
  });
});
