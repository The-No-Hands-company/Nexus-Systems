import { describe, expect, test } from "bun:test";
import { readFileSync, existsSync } from "node:fs";
import { join } from "node:path";
import { vendorTargets } from "../scripts/vendor";

const root = join(import.meta.dirname, "..", "..", "..");

describe("vendored token copies", () => {
  const generated = () =>
    readFileSync(join(root, "packages/nexus-design/dist/nexus-tokens.css"), "utf-8");

  test("names at least the two standalone repos", () => {
    const dests = vendorTargets().map((t) => t.repo);
    expect(dests).toContain("apps/Nexus");
    expect(dests).toContain("apps/Nexus-Cloud");
  });

  for (const target of vendorTargets()) {
    test(`${target.repo} has a vendored copy`, () => {
      expect(existsSync(join(root, target.dest))).toBe(true);
    });

    // The one that matters. A vendored file that has drifted from the
    // generator is the exact failure this whole pipeline exists to prevent,
    // and it is invisible until someone notices two apps looking different.
    test(`${target.repo}'s copy is byte-identical to the generated output`, () => {
      expect(readFileSync(join(root, target.dest), "utf-8")).toBe(generated());
    });
  }
});
