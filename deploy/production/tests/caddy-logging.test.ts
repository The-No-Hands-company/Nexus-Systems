import { describe, expect, test } from "bun:test";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";

const caddyfiles = [
  resolve(import.meta.dir, "..", "nexus-chat.Caddyfile"),
  resolve(import.meta.dir, "..", "..", "..", "apps", "Nexus", "Caddyfile"),
];

describe("Caddy access-log redaction", () => {
  for (const caddyfile of caddyfiles) {
    test(`${caddyfile} removes request headers before encoding`, () => {
      const config = readFileSync(caddyfile, "utf8");

      expect(config).toMatch(
        /format\s+filter\s*\{\s*fields\s*\{\s*request>headers\s+delete\s*\}\s*wrap\s+(?:console|json)\s*\}/s,
      );
      expect(config).not.toMatch(/format\s+(?:console|json)\b/);
    });
  }
});
