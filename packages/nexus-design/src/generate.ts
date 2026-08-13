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

const here = dirname(fileURLToPath(import.meta.url));
const dist = join(here, "..", "dist");
const pairs = flattenTokens(tokens as Record<string, unknown>);
mkdirSync(dist, { recursive: true });
writeFileSync(join(dist, "nexus-tokens.css"), renderTokensCss(pairs));
writeFileSync(join(dist, "nexus-theme.css"), renderThemeCss(pairs));
console.log(`[nexus-design] wrote ${pairs.length} tokens to dist/`);
