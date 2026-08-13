/** Keys that describe the token file itself rather than any design value. */
const METADATA_KEYS = new Set(["$schema", "version", "theme"]);

export interface TokenPair {
  name: string;
  value: string;
}

/**
 * Turn nested token JSON into flat CSS custom property pairs.
 *
 * Depth-first so output order follows the source file, which makes the
 * generated CSS reviewable against the JSON side by side.
 */
export function flattenTokens(
  tokens: Record<string, unknown>,
  prefix = "--nexus",
): TokenPair[] {
  const out: TokenPair[] = [];
  for (const [key, value] of Object.entries(tokens)) {
    if (prefix === "--nexus" && METADATA_KEYS.has(key)) continue;
    const name = `${prefix}-${key}`;
    if (value !== null && typeof value === "object" && !Array.isArray(value)) {
      out.push(...flattenTokens(value as Record<string, unknown>, name));
    } else {
      out.push({ name, value: String(value) });
    }
  }
  return out;
}
