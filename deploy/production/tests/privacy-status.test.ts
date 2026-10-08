import { describe, it, expect, afterEach } from "bun:test";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { privacyStatusBody } from "../privacy-status";

const dir = mkdtempSync(join(tmpdir(), "privacy-status-"));
const file = (o: unknown) => { const p = join(dir, `r-${Math.random()}.json`); writeFileSync(p, typeof o === "string" ? o : JSON.stringify(o)); return p; };
const NOW = Date.parse("2026-10-08T12:00:00Z");
const sample = (over: Record<string, unknown> = {}) => ({
  at: "2026-10-08T00:01:00Z", address: "203.0.113.77", status: "pass", found: [],
  sources: { logs: 28, containers: 24, databases: 6, files: 5, probes_answered: 21 }, ...over,
});

describe("privacyStatusBody", () => {
  it("publishes only the safe fields for a pass", async () => {
    const body = await privacyStatusBody(file(sample()), NOW);
    expect(body).toEqual({ status: "pass", checkedAt: "2026-10-08T00:01:00Z",
      searched: { databases: 6, containers: 24, logs: 28, files: 5, probes: 21 }, findings: 0 });
  });
  it("reports a failure with a count, never the locations or the address", async () => {
    const body = await privacyStatusBody(file(sample({ status: "fail", found: ["db:nexus_chat", "log:/tmp/x.log"] })), NOW);
    expect(body.status).toBe("fail");
    expect(body.findings).toBe(2);
    const text = JSON.stringify(body);
    for (const leak of ["nexus_chat", "/tmp/x.log", "203.0.113.77", "address", "found"]) expect(text).not.toContain(leak);
  });
  it("is stale when the file is missing, unparsable or older than 36 hours", async () => {
    expect(await privacyStatusBody(join(dir, "missing.json"), NOW)).toEqual({ status: "stale", findings: 0 });
    expect(await privacyStatusBody(file("{not json"), NOW)).toEqual({ status: "stale", findings: 0 });
    expect(await privacyStatusBody(file(sample({ at: "2026-10-06T23:59:00Z" })), NOW)).toEqual({ status: "stale", findings: 0 });
  });
  it("treats an unknown status as stale rather than pass", async () => {
    expect(await privacyStatusBody(file(sample({ status: "weird" })), NOW)).toEqual({ status: "stale", findings: 0 });
  });
});

describe("status.tnhc.dev route", () => {
  const saved = process.env.PRIVACY_CANARY_RESULT;
  afterEach(() => { if (saved === undefined) delete process.env.PRIVACY_CANARY_RESULT; else process.env.PRIVACY_CANARY_RESULT = saved; });
  it("serves the JSON ungated with cache and CORS headers", async () => {
    process.env.PRIVACY_CANARY_RESULT = file(sample({ at: new Date().toISOString() }));
    const { handleRequest } = await import("../proxy");
    const prev = process.env.GATE_SKIP_AUTH; delete process.env.GATE_SKIP_AUTH;
    try {
      const res = await handleRequest(new Request("http://status.tnhc.dev/privacy.json"));
      expect(res.status).toBe(200);
      expect(res.headers.get("access-control-allow-origin")).toBe("https://tnhc.dev");
      expect(res.headers.get("cache-control")).toBe("public, max-age=300");
      expect((await res.json()).status).toBe("pass");
    } finally { if (prev !== undefined) process.env.GATE_SKIP_AUTH = prev; }
  });
  it("404s other paths and 405s other methods", async () => {
    const { handleRequest } = await import("../proxy");
    expect((await handleRequest(new Request("http://status.tnhc.dev/"))).status).toBe(404);
    expect((await handleRequest(new Request("http://status.tnhc.dev/../etc/passwd"))).status).toBe(404);
    expect((await handleRequest(new Request("http://status.tnhc.dev/privacy.json", { method: "POST" }))).status).toBe(405);
  });
});
