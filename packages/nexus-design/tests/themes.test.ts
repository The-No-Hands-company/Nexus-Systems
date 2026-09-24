import { describe, expect, test } from "bun:test";
import void_ from "../tokens/themes/void.json";
import abyss from "../tokens/themes/abyss.json";
import petrol from "../tokens/themes/petrol.json";
import slate from "../tokens/themes/slate.json";
import balanced from "../tokens/density/balanced.json";
import compact from "../tokens/density/compact.json";
import { flattenTokens } from "../src/flatten";

const THEMES = { void: void_, abyss, petrol, slate } as Record<string, Record<string, unknown>>;
const DENSITIES = { balanced, compact } as Record<string, Record<string, unknown>>;

function names(obj: Record<string, unknown>): string[] {
  return flattenTokens(obj).map((p) => p.name).sort();
}

describe("theme family", () => {
  test("every theme declares exactly the same token names", () => {
    const reference = names(THEMES.void!);
    expect(reference.length).toBeGreaterThan(0);
    for (const [id, theme] of Object.entries(THEMES)) {
      expect({ id, names: names(theme) }).toEqual({ id, names: reference });
    }
  });

  test("every density declares exactly the same token names", () => {
    const reference = names(DENSITIES.balanced!);
    for (const [id, density] of Object.entries(DENSITIES)) {
      expect({ id, names: names(density) }).toEqual({ id, names: reference });
    }
  });

  test("themes carry colour only, densities carry no colour", () => {
    for (const theme of Object.values(THEMES)) {
      expect(Object.keys(theme)).toEqual(["color"]);
    }
    for (const density of Object.values(DENSITIES)) {
      expect(Object.keys(density)).not.toContain("color");
    }
  });

  test("the base token file no longer carries anything a theme or density owns", () => {
    const base = require("../tokens/nexus.tokens.json") as Record<string, unknown>;
    expect(base).not.toHaveProperty("color");
    expect(base).not.toHaveProperty("space");
    expect((base.typography as Record<string, unknown>)).not.toHaveProperty("size");
  });
});
