import { describe, expect, test } from "bun:test";
import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

const UI = join(import.meta.dir, "..", "src", "components", "ui");

function sources(): Array<{ file: string; text: string }> {
  return readdirSync(UI)
    .filter((f) => f.endsWith(".tsx") || f.endsWith(".ts"))
    .map((f) => ({ file: f, text: readFileSync(join(UI, f), "utf8") }));
}

describe("the component kit reads tokens only", () => {
  test("no file contains a hex colour literal", () => {
    const offenders = sources()
      .map(({ file, text }) => ({ file, hits: text.match(/#[0-9a-fA-F]{3,8}\b/g) ?? [] }))
      .filter(({ hits }) => hits.length > 0);
    expect(offenders).toEqual([]);
  });

  test("no file contains a raw rgba() literal", () => {
    const offenders = sources()
      .map(({ file, text }) => ({ file, hits: text.match(/rgba?\(/g) ?? [] }))
      .filter(({ hits }) => hits.length > 0);
    expect(offenders).toEqual([]);
  });

  // #hex and rgba() are not the only way a raw colour sneaks in — Tailwind's
  // bare white/black utilities (border-white/10, placeholder:text-white/30)
  // are colour literals too, and the two tests above cannot see them.
  test("no file contains a bare white/black Tailwind colour utility", () => {
    const offenders = sources()
      .map(({ file, text }) => ({ file, hits: text.match(/[-:](white|black)(\/\d{1,3})?\b/g) ?? [] }))
      .filter(({ hits }) => hits.length > 0);
    expect(offenders).toEqual([]);
  });

  test("there is exactly one copy of cn()", () => {
    const defs = sources().filter(({ text }) => /function cn\(/.test(text));
    expect(defs.map((d) => d.file)).toEqual(["cn.ts"]);
  });
});

import { initials } from "../src/components/ui/avatar";

describe("Avatar initials", () => {
  test("takes the first letter of the first two words", () => {
    expect(initials("Eric Hakansson")).toBe("EH");
  });
  test("falls back to the first two characters of a single word", () => {
    expect(initials("nexus")).toBe("NE");
  });
  test("survives empty and whitespace input rather than rendering nothing", () => {
    expect(initials("")).toBe("?");
    expect(initials("   ")).toBe("?");
  });
});

describe("kit barrel", () => {
  test("every component file is re-exported from src/index.ts", async () => {
    const barrel = readFileSync(join(UI, "..", "..", "index.ts"), "utf8");
    for (const { file } of sources()) {
      if (file === "cn.ts") continue;
      const stem = file.replace(/\.tsx?$/, "");
      expect(barrel).toContain(`./components/ui/${stem}`);
    }
  });

  // The test above only proves a file's path string appears somewhere in
  // index.ts — it passed while Card's subcomponents (CardHeader, CardTitle,
  // CardDescription, CardContent, CardFooter) were unreachable through the
  // barrel, because `export { Card } from "./components/ui/card"` contains
  // the path but names only one of six symbols the file defines. This
  // imports the real module and checks each symbol actually exists.
  test("the barrel actually exports every symbol, not just a path string", async () => {
    const mod: Record<string, unknown> = await import("../src/index.ts");
    const expected = [
      "cn", "Button", "Card", "CardHeader", "CardTitle", "CardDescription",
      "CardContent", "CardFooter", "Input", "Pill", "Kbd", "Avatar", "initials",
      "Overlay", "EmptyState", "Skeleton",
    ];
    for (const name of expected) {
      expect(mod[name]).toBeDefined();
    }
  });
});
