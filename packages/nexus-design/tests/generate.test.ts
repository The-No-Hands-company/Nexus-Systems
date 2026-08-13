import { describe, expect, test } from "bun:test";
import tokens from "../tokens/nexus.tokens.json";
import { flattenTokens } from "../src/flatten";
import { renderTokensCss, renderThemeCss } from "../src/generate";

const pairs = flattenTokens(tokens as Record<string, unknown>);

describe("generated CSS", () => {
  test("every token reaches nexus-tokens.css", () => {
    // The drift guard. Source and output disagreeing is the failure this
    // pipeline exists to prevent, so it is checked exhaustively, not sampled.
    const css = renderTokensCss(pairs);
    for (const { name, value } of pairs) {
      expect(css).toContain(`${name}: ${value};`);
    }
  });

  test("every token reaches nexus-theme.css", () => {
    const css = renderThemeCss(pairs);
    for (const { name } of pairs) {
      expect(css).toContain(name.replace("--nexus-", "--"));
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
    expect(renderTokensCss(pairs)).toContain("--nexus-color-accent-primary: #27c9a5;");
  });

  test("both outputs say they are generated", () => {
    expect(renderTokensCss(pairs)).toContain("Generated");
    expect(renderThemeCss(pairs)).toContain("Generated");
  });
});
