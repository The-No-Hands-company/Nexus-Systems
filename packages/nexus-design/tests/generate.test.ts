import { describe, expect, test } from "bun:test";
import tokens from "../tokens/nexus.tokens.json";
import voidTheme from "../tokens/themes/void.json";
import balancedDensity from "../tokens/density/balanced.json";
import { flattenTokens } from "../src/flatten";
import { renderTokensCss, renderThemeCss, renderScopedCss } from "../src/generate";

const pairs = flattenTokens(tokens as Record<string, unknown>);

/**
 * Base + the void theme + the balanced density — the same combination the
 * build script feeds to renderThemeCss/renderTokensCss for its single-theme
 * outputs. Colour, spacing and font-size tokens no longer live in the base
 * file (Task 1 moved them out into themes/densities), so any test that wants
 * to see one of those needs this merged list instead of `pairs` alone.
 */
const themedPairs = [
  ...pairs,
  ...flattenTokens(voidTheme as Record<string, unknown>),
  ...flattenTokens(balancedDensity as Record<string, unknown>),
];

/**
 * Mirrors the documented unit/namespace rules independently of generate.ts,
 * so the exhaustive drift tests below assert against the spec rather than
 * against whatever the implementation happens to do.
 */
function isNumericValue(value: string): boolean {
  return /^-?\d+(\.\d+)?$/.test(value);
}

function pathOf(name: string): string[] {
  return name.replace(/^--nexus-/, "").split("-");
}

function expectedValue(name: string, rawValue: string): string {
  if (!isNumericValue(rawValue)) return rawValue;
  const [group, sub] = pathOf(name);
  if (group === "space" || group === "radius") return `${rawValue}px`;
  if (group === "typography" && sub === "size") return `${rawValue}px`;
  if (group === "motion" && sub === "duration") return `${rawValue}ms`;
  return rawValue;
}

function expectedThemeName(name: string): string {
  const [group, ...rest] = pathOf(name);
  if (group === "color") return `--color-${rest.join("-")}`;
  if (group === "radius") return `--radius-${rest.join("-")}`;
  if (group === "shadow") return `--shadow-${rest.join("-")}`;
  if (group === "space") return `--spacing-${rest.join("-")}`;
  if (group === "typography") {
    const [sub, ...tail] = rest;
    if (sub === "size") return `--text-${tail.join("-")}`;
    if (sub === "fontFamily") return `--font-${tail.join("-")}`;
    if (sub === "weight") return `--font-weight-${tail.join("-")}`;
    if (sub === "lineHeight") return `--leading-${tail.join("-")}`;
  }
  if (group === "motion") {
    const [sub, ...tail] = rest;
    if (sub === "easing") return `--ease-${tail.join("-")}`;
  }
  return `--${pathOf(name).join("-")}`;
}

describe("generated CSS", () => {
  test("every token reaches nexus-tokens.css with the correct unit", () => {
    // The drift guard. Source and output disagreeing is the failure this
    // pipeline exists to prevent, so it is checked exhaustively, not sampled.
    const css = renderTokensCss(pairs);
    for (const { name, value } of pairs) {
      expect(css).toContain(`${name}: ${expectedValue(name, value)};`);
    }
  });

  test("every token reaches nexus-theme.css under its v4 namespace", () => {
    const css = renderThemeCss(pairs);
    for (const { name, value } of pairs) {
      expect(css).toContain(`${expectedThemeName(name)}: ${expectedValue(name, value)};`);
    }
  });

  test("tokens css targets :root so any document picks it up", () => {
    expect(renderTokensCss(pairs)).toContain(":root {");
  });

  test("theme css uses a Tailwind v4 @theme block, not a config object", () => {
    // v4 has no tailwind.config.js. A JS preset here would be silently ignored.
    expect(renderThemeCss(pairs)).toContain("@theme {");
  });

  test("the accent colour survives the whole pipeline intact", () => {
    // Colour now comes from a theme (void), not the base file.
    expect(renderTokensCss(themedPairs)).toContain("--nexus-color-accent-primary: #CCFF00;");
  });

  test("both outputs say they are generated", () => {
    expect(renderTokensCss(pairs)).toContain("Generated");
    expect(renderThemeCss(pairs)).toContain("Generated");
  });

  test("a spacing token emerges with a px unit, not a bare number", () => {
    // Spacing now comes from a density (balanced), not the base file.
    expect(renderTokensCss(themedPairs)).toContain("--nexus-space-4: 16px;");
  });

  test("a weight token stays unitless", () => {
    expect(renderTokensCss(pairs)).toContain("--nexus-typography-weight-regular: 400;");
  });

  test("a lineHeight token stays unitless", () => {
    expect(renderTokensCss(pairs)).toContain("--nexus-typography-lineHeight-tight: 1.2;");
  });

  test("a zIndex token stays unitless", () => {
    expect(renderTokensCss(pairs)).toContain("--nexus-zIndex-base: 1;");
  });

  test("a duration token gets an ms unit", () => {
    expect(renderTokensCss(pairs)).toContain("--nexus-motion-duration-fast: 120ms;");
  });

  test("a radius token gets a px unit", () => {
    expect(renderTokensCss(pairs)).toContain("--nexus-radius-sm: 6px;");
  });

  test("space.4 lands under Tailwind's --spacing-* namespace in the theme output", () => {
    // Spacing now comes from a density (balanced), not the base file.
    expect(renderThemeCss(themedPairs)).toContain("--spacing-4: 16px;");
  });

  test("typography.size.sm lands under Tailwind's --text-* namespace in the theme output", () => {
    // Font sizes now come from a density (balanced), not the base file.
    expect(renderThemeCss(themedPairs)).toContain("--text-sm: 13px;");
  });

  test("typography.fontFamily, weight and lineHeight land under their v4 namespaces", () => {
    // fontFamily/weight/lineHeight still live in the base file, but the
    // assertion runs against themedPairs to match the pipeline's actual input.
    const css = renderThemeCss(themedPairs);
    expect(css).toContain("--font-base: Satoshi, -apple-system, BlinkMacSystemFont, sans-serif;");
    expect(css).toContain("--font-weight-regular: 400;");
    expect(css).toContain("--leading-tight: 1.2;");
  });

  test("motion.easing lands under Tailwind's --ease-* namespace", () => {
    expect(renderThemeCss(pairs)).toContain("--ease-standard: cubic-bezier(0.2,0,0,1);");
  });

  test("radius and shadow keep their existing namespaces", () => {
    const css = renderThemeCss(pairs);
    expect(css).toContain("--radius-sm: 6px;");
    expect(css).toContain("--shadow-sm: 0 2px 8px rgba(0,0,0,0.18);");
  });

  test("motion.duration and zIndex have no v4 namespace and land as plain custom properties", () => {
    const css = renderThemeCss(pairs);
    expect(css).toContain("--motion-duration-fast: 120ms;");
    expect(css).toContain("--zIndex-base: 1;");
    expect(css).toContain("not a utility");
  });
});

describe("scoped css", () => {
  const base = [{ name: "--nexus-radius-md", value: "10" }];
  const themes = {
    void: [{ name: "--nexus-color-bg-canvas", value: "#030303" }],
    abyss: [{ name: "--nexus-color-bg-canvas", value: "#04100E" }],
  };
  const densities = {
    balanced: [{ name: "--nexus-space-4", value: "16" }],
    compact: [{ name: "--nexus-space-4", value: "12" }],
  };
  const css = renderScopedCss(base, themes, densities, { theme: "void", density: "balanced" });

  test("the default theme is also written to bare :root", () => {
    // A document with no data-nexus-theme must render Void, not nothing.
    expect(css).toMatch(/:root,\s*\[data-nexus-theme="void"\]\s*\{[^}]*#030303/);
  });

  test("the default density is also written to bare :root", () => {
    expect(css).toMatch(/:root,\s*\[data-nexus-density="balanced"\]\s*\{[^}]*16px/);
  });

  test("non-default themes are scoped only to their attribute", () => {
    expect(css).toMatch(/\[data-nexus-theme="abyss"\]\s*\{[^}]*#04100E/);
    expect(css).not.toMatch(/:root,\s*\[data-nexus-theme="abyss"\]/);
  });

  test("base tokens are emitted once, unscoped", () => {
    expect(css.match(/--nexus-radius-md/g)).toHaveLength(1);
  });

  test("density values still receive their unit", () => {
    expect(css).toContain("--nexus-space-4: 12px;");
  });
});
