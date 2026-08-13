import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { flattenTokens, type TokenPair } from "./flatten";
import tokens from "../tokens/nexus.tokens.json";

const BANNER = `/* Generated from tokens/nexus.tokens.json — do not edit.
   Change the JSON and run: bun run build */`;

/** Plain custom properties. Works in any document, no build step required. */
export function renderTokensCss(pairs: TokenPair[]): string {
  const body = pairs.map((p) => `  ${p.name}: ${p.value};`).join("\n");
  return `${BANNER}\n:root {\n${body}\n}\n`;
}

/**
 * Tailwind v4 theme.
 *
 * v4 reads its theme from an `@theme` block in CSS — there is no config file
 * to put a preset in. The `--nexus-` prefix is dropped here because Tailwind
 * derives utility names from the variable name: `--color-accent-primary`
 * becomes `bg-accent-primary`.
 */
export function renderThemeCss(pairs: TokenPair[]): string {
  const body = pairs
    .map((p) => `  ${p.name.replace("--nexus-", "--")}: ${p.value};`)
    .join("\n");
  return `${BANNER}\n@theme {\n${body}\n}\n`;
}

const here = dirname(fileURLToPath(import.meta.url));
const dist = join(here, "..", "dist");
const pairs = flattenTokens(tokens as Record<string, unknown>);
mkdirSync(dist, { recursive: true });
writeFileSync(join(dist, "nexus-tokens.css"), renderTokensCss(pairs));
writeFileSync(join(dist, "nexus-theme.css"), renderThemeCss(pairs));
console.log(`[nexus-design] wrote ${pairs.length} tokens to dist/`);
