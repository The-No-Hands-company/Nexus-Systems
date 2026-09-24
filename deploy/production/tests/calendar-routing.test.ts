import { describe, expect, it } from "bun:test";

const caddyfile = await Bun.file(new URL("../nexus-calendar.Caddyfile", import.meta.url)).text();

describe("Calendar production front door", () => {
  it("keeps public delivery to the explicit share document and token API", () => {
    expect(caddyfile).toContain("handle /api/v1/calendar/public/*");
    expect(caddyfile).toContain("handle /api/v1/calendar/*");
    expect(caddyfile).toContain("handle /share/*");
  });

  it("serves one cache-safe SPA artifact without letting fallback swallow APIs", () => {
    expect(caddyfile).toContain("handle /assets/*");
    expect(caddyfile).toContain("try_files {path} /index.html");
    expect(caddyfile.indexOf("handle /api/v1/calendar/*")).toBeLessThan(caddyfile.indexOf("try_files {path} /index.html"));
    expect(caddyfile).toContain('Cache-Control "public, max-age=31536000, immutable"');
    expect(caddyfile).toContain('Cache-Control "no-cache, no-store, must-revalidate"');
  });

  it("sets the direct-origin security policy, including no-referrer public pages", () => {
    expect(caddyfile).toContain('Content-Security-Policy "default-src \'self\'; frame-ancestors \'self\' https://app.tnhc.dev; base-uri \'self\'; object-src \'none\'"');
    expect(caddyfile).toContain('Referrer-Policy "no-referrer"');
    expect(caddyfile).toContain('X-Content-Type-Options "nosniff"');
  });
});
