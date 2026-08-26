import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { flattenTokens, type TokenPair } from "./flatten";
import tokens from "../tokens/nexus.tokens.json";

const BANNER = `/* Generated from tokens/nexus.tokens.json — do not edit.
   Change the JSON and run: bun run build */`;

/** True for strings that came from a JSON number (flatten stringifies everything). */
function isNumericValue(value: string): boolean {
  return /^-?\d+(\.\d+)?$/.test(value);
}

/** A token's path segments, e.g. "--nexus-typography-size-xs" -> ["typography","size","xs"]. */
function pathOf(name: string): string[] {
  return name.replace(/^--nexus-/, "").split("-");
}

/**
 * CSS unit for a numeric token, chosen by its top-level group.
 *
 * space/radius/typography.size are pixel scales; motion.duration is a time.
 * typography.weight, typography.lineHeight and zIndex are genuinely unitless
 * in CSS — appending a unit to those would break them.
 */
function unitFor(path: string[]): string {
  const [group, sub] = path;
  if (group === "space" || group === "radius") return "px";
  if (group === "typography" && sub === "size") return "px";
  if (group === "motion" && sub === "duration") return "ms";
  return "";
}

/** Attach the correct unit to every numeric pair; non-numeric values pass through untouched. */
function withUnits(pairs: TokenPair[]): TokenPair[] {
  return pairs.map((p) => {
    if (!isNumericValue(p.value)) return p;
    const unit = unitFor(pathOf(p.name));
    return unit ? { name: p.name, value: `${p.value}${unit}` } : p;
  });
}

/** Plain custom properties. Works in any document, no build step required. */
export function renderTokensCss(pairs: TokenPair[]): string {
  const body = withUnits(pairs)
    .map((p) => `  ${p.name}: ${p.value};`)
    .join("\n");
  return `${BANNER}\n:root {\n${body}\n}\n`;
}

/**
 * Map a token's path to the Tailwind v4 theme namespace it actually powers.
 *
 * v4 derives utilities from the variable name (`--color-*` -> `bg-*`/`text-*`,
 * `--spacing-*` -> `p-*`/`gap-*`, `--text-*` -> font-size utilities, etc.), so
 * the rename has to land on v4's own namespace vocabulary, not just drop the
 * `--nexus-` prefix. Groups with no v4 namespace fall through to `--${path}`,
 * a plain custom property `var()` can still reach.
 */
function themeVarName(path: string[]): string {
  const [group, ...rest] = path;
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
  return `--${path.join("-")}`;
}

/** True for token groups with no Tailwind v4 theme namespace (motion.duration, zIndex). */
function hasNoNamespace(path: string[]): boolean {
  return (path[0] === "motion" && path[1] === "duration") || path[0] === "zIndex";
}

/**
 * Tailwind v4 theme.
 *
 * v4 reads its theme from an `@theme` block in CSS — there is no config file
 * to put a preset in. Each token is renamed to the v4 namespace it actually
 * powers rather than just having `--nexus-` stripped, since only some paths
 * happen to line up with v4's namespace names. motion.duration and zIndex
 * have no v4 namespace at all, so they're emitted as plain custom properties
 * — not utilities, but still reachable via `var()`.
 */
export function renderThemeCss(pairs: TokenPair[]): string {
  const lines: string[] = [];
  let prevNoNamespace = false;
  for (const p of withUnits(pairs)) {
    const path = pathOf(p.name);
    const noNamespace = hasNoNamespace(path);
    if (noNamespace && !prevNoNamespace) {
      lines.push(
        "  /* No Tailwind v4 theme namespace for this group — plain custom property, not a utility. */",
      );
    }
    lines.push(`  ${themeVarName(path)}: ${p.value};`);
    prevNoNamespace = noNamespace;
  }
  return `${BANNER}\n@theme {\n${lines.join("\n")}\n}\n`;
}

import { validateTheme, type Finding } from "./contrast";

/** One selector's worth of declarations. */
function block(selector: string, pairs: TokenPair[]): string {
  const body = withUnits(pairs).map((p) => `  ${p.name}: ${p.value};`).join("\n");
  return `${selector} {\n${body}\n}\n`;
}

/**
 * Theme- and density-scoped custom properties.
 *
 * The defaults are emitted on bare `:root` *as well as* their own attribute, so
 * a document that never sets data-nexus-theme still renders Void rather than a
 * page with no colours at all.
 */
export function renderScopedCss(
  base: TokenPair[],
  themes: Record<string, TokenPair[]>,
  densities: Record<string, TokenPair[]>,
  defaults: { theme: string; density: string },
): string {
  const out = [BANNER, "", block(":root", base)];

  for (const [id, pairs] of Object.entries(themes)) {
    const selector = id === defaults.theme
      ? `:root, [data-nexus-theme="${id}"]`
      : `[data-nexus-theme="${id}"]`;
    out.push(block(selector, pairs));
  }
  for (const [id, pairs] of Object.entries(densities)) {
    const selector = id === defaults.density
      ? `:root, [data-nexus-density="${id}"]`
      : `[data-nexus-density="${id}"]`;
    out.push(block(selector, pairs));
  }
  return out.join("\n");
}

export function renderContrastReport(findings: Finding[]): string {
  const rows = findings
    .map((f) => `| ${f.themeId} | ${f.pair} | ${f.ratio.toFixed(2)} | ${f.required} | ${f.ok ? "pass" : "**FAIL**"} |`)
    .join("\n");
  return [
    "# Contrast report",
    "",
    "Generated by `bun run build`. Body text requires 4.5:1; muted text, state",
    "colours and the accent are non-text UI at 3:1.",
    "",
    "| Theme | Pair | Ratio | Required | |",
    "|---|---|---|---|---|",
    rows,
    "",
  ].join("\n");
}

const here = dirname(fileURLToPath(import.meta.url));
const dist = join(here, "..", "dist");
const tokensDir = join(here, "..", "tokens");

const THEME_IDS = ["void", "abyss", "petrol", "slate"] as const;
const DENSITY_IDS = ["balanced", "compact"] as const;

async function loadJson(path: string): Promise<Record<string, unknown>> {
  return JSON.parse(await Bun.file(path).text()) as Record<string, unknown>;
}

const base = flattenTokens(tokens as Record<string, unknown>);

const themeJson: Record<string, Record<string, unknown>> = {};
const themes: Record<string, TokenPair[]> = {};
for (const id of THEME_IDS) {
  themeJson[id] = await loadJson(join(tokensDir, "themes", `${id}.json`));
  themes[id] = flattenTokens(themeJson[id]!);
}

const densities: Record<string, TokenPair[]> = {};
for (const id of DENSITY_IDS) {
  densities[id] = flattenTokens(await loadJson(join(tokensDir, "density", `${id}.json`)));
}

// Contrast gate. Runs before anything is written, so a failing palette cannot
// leave stale-but-valid CSS on disk looking like a successful build.
const findings: Finding[] = [];
for (const id of THEME_IDS) {
  const colors = Object.fromEntries(
    flattenTokens(themeJson[id]!)
      .map((p) => [p.name.replace("--nexus-color-", "").replace(/-/g, "."), p.value]),
  ) as Record<string, string>;
  findings.push(...validateTheme(id, colors));
}
const failures = findings.filter((f) => !f.ok);

mkdirSync(dist, { recursive: true });
writeFileSync(join(dist, "contrast-report.md"), renderContrastReport(findings));

if (failures.length > 0) {
  for (const f of failures) {
    console.error(`[nexus-design] ${f.themeId}: ${f.pair} is ${f.ratio} (needs ${f.required})`);
  }
  throw new Error(`contrast gate failed: ${failures.length} pair(s) below AA`);
}

writeFileSync(
  join(dist, "nexus-tokens.css"),
  renderScopedCss(base, themes, densities, { theme: "void", density: "balanced" }),
);
writeFileSync(join(dist, "nexus-theme.css"), renderThemeCss([...base, ...themes.void!, ...densities.balanced!]));
console.log(`[nexus-design] wrote ${THEME_IDS.length} themes, ${DENSITY_IDS.length} densities, ${findings.length} contrast pairs`);
