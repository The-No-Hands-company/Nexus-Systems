import { describe, expect, test } from "bun:test";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";

const caddyfiles = [
  resolve(import.meta.dir, "..", "nexus-chat.Caddyfile"),
  resolve(import.meta.dir, "..", "nexus-draw.Caddyfile"),
  resolve(import.meta.dir, "..", "nexus-calendar.Caddyfile"),
];

describe("Caddy request logging is off", () => {
  for (const caddyfile of caddyfiles) {
    test(`${caddyfile} has no access log directive`, () => {
      // Strip comments, then assert no `log` directive (which would turn on
      // per-request access logging) and no log output/format configuration.
      const config = readFileSync(caddyfile, "utf8")
        .split("\n")
        .map((line) => line.replace(/#.*$/, ""))
        .join("\n");

      expect(config).not.toMatch(/^\s*log\b/m);
      expect(config).not.toMatch(/^\s*output\s+(?:stdout|stderr|file)\b/m);
      expect(config).not.toMatch(/^\s*format\s+(?:console|json|filter)\b/m);
    });
  }
});
