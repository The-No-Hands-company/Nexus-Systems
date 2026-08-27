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

  test("there is exactly one copy of cn()", () => {
    const defs = sources().filter(({ text }) => /function cn\(/.test(text));
    expect(defs.map((d) => d.file)).toEqual(["cn.ts"]);
  });
});
