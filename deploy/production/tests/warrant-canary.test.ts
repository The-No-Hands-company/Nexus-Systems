import { describe, it, expect, afterEach } from "bun:test";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { canaryStatusBody } from "../warrant-canary";

const dir = mkdtempSync(join(tmpdir(), "warrant-canary-"));
const canaryFile = (date: string, extra = "") => {
  const p = join(dir, `canary-${Math.random()}.md`);
  writeFileSync(p, `# Warrant canary\n\n**Last updated:** ${date}\n\n${extra}`);
  return p;
};
const NOW = Date.parse("2026-10-16T12:00:00Z");

describe("canaryStatusBody", () => {
  it("returns active with correct fields for a fresh canary", async () => {
    const body = await canaryStatusBody(canaryFile("2026-10-16"), NOW);
    expect(body.status).toBe("active");
    expect(body.issuedAt).toBe("2026-10-16T00:00:00Z");
    expect(typeof body.daysRemaining).toBe("number");
    expect(body.daysRemaining).toBeGreaterThan(0);
    expect(body.scope).toBeTruthy();
  });

  it("returns stale when the last update is > 90 days ago", async () => {
    // 91 days before NOW
    const body = await canaryStatusBody(canaryFile("2026-07-16"), NOW);
    expect(body.status).toBe("stale");
    expect(body.daysRemaining).toBeLessThan(0);
  });

  it("returns unavailable when the file is missing", async () => {
    const body = await canaryStatusBody(join(dir, "missing.md"), NOW);
    expect(body.status).toBe("unavailable");
    expect(body.issuedAt).toBeUndefined();
  });

  it("returns unavailable when the file has no last-updated line", async () => {
    const p = join(dir, "no-date.md");
    writeFileSync(p, "# Warrant canary\n\nNo date here.\n");
    const body = await canaryStatusBody(p, NOW);
    expect(body.status).toBe("unavailable");
  });

  it("returns unavailable when the date is more than 24 hours in the future", async () => {
    const body = await canaryStatusBody(canaryFile("2026-10-18"), NOW);
    expect(body.status).toBe("unavailable");
  });

  it("does not leak the canary text in the JSON output", async () => {
    const body = await canaryStatusBody(canaryFile("2026-10-16", "secret text"), NOW);
    expect(JSON.stringify(body)).not.toContain("secret text");
  });
});

describe("status.tnhc.dev /canary.json route", () => {
  const savedEnv = process.env.WARRANT_CANARY_PATH;
  afterEach(() => {
    if (savedEnv === undefined) delete process.env.WARRANT_CANARY_PATH;
    else process.env.WARRANT_CANARY_PATH = savedEnv;
  });

  it("serves the canary JSON ungated with correct headers", async () => {
    process.env.WARRANT_CANARY_PATH = canaryFile("2026-10-16");
    const { handleRequest } = await import("../proxy");
    const res = await handleRequest(new Request("http://status.tnhc.dev/canary.json"));
    expect(res.status).toBe(200);
    expect(res.headers.get("access-control-allow-origin")).toBe("https://tnhc.dev");
    expect(res.headers.get("cache-control")).toBe("public, max-age=3600");
    const json = await res.json();
    expect(json.status).toBe("active");
    expect(json.issuedAt).toBeTruthy();
    // Must not expose private document content.
    expect(JSON.stringify(json)).not.toContain("Statement");
  });

  it("404s unknown paths under status.tnhc.dev", async () => {
    const { handleRequest } = await import("../proxy");
    expect((await handleRequest(new Request("http://status.tnhc.dev/other.json"))).status).toBe(404);
    expect((await handleRequest(new Request("http://status.tnhc.dev/../etc/passwd"))).status).toBe(404);
  });

  it("still serves /privacy.json with its own cache-control", async () => {
    process.env.PRIVACY_CANARY_RESULT = join(dir, "missing-privacy.json");
    const { handleRequest } = await import("../proxy");
    const res = await handleRequest(new Request("http://status.tnhc.dev/privacy.json"));
    expect(res.status).toBe(200);
    expect(res.headers.get("cache-control")).toBe("public, max-age=300");
  });

  it("answers OPTIONS for /canary.json with 204 and CORS headers", async () => {
    const { handleRequest } = await import("../proxy");
    const res = await handleRequest(new Request("http://status.tnhc.dev/canary.json", { method: "OPTIONS" }));
    expect(res.status).toBe(204);
    expect(res.headers.get("access-control-allow-origin")).toBe("https://tnhc.dev");
  });
});
