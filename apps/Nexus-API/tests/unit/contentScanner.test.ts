import { describe, it, expect, vi } from "vitest";

/**
 * Tests for lib/contentScanner.ts — defines the webhook contract that
 * any scanner integration must conform to.
 */

interface ScanTarget {
  deploymentId: number; siteId: number; siteDomain: string;
  fileCount: number; totalSizeMb: number;
  files: Array<{ path: string; contentType: string; sizeBytes: number; objectPath: string }>;
}
interface ScanResult { safe: boolean; reason?: string; flaggedFiles?: string[]; skipped: boolean; }

function makeScanner(webhookUrl: string, failClosed: boolean) {
  return async function scan(target: ScanTarget): Promise<ScanResult> {
    if (!webhookUrl) return { safe: true, skipped: true };
    try {
      const res = await fetch(webhookUrl, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(target),
      });
      if (!res.ok) {
        return failClosed
          ? { safe: false, skipped: false, reason: `Scanner returned HTTP ${res.status}` }
          : { safe: true, skipped: false };
      }
      const data = await res.json() as { safe?: boolean; reason?: string; flaggedFiles?: string[] };
      if (data.safe === false) return { safe: false, skipped: false, reason: data.reason, flaggedFiles: data.flaggedFiles };
      return { safe: true, skipped: false };
    } catch {
      return failClosed
        ? { safe: false, skipped: false, reason: "Content scanner unreachable" }
        : { safe: true, skipped: false };
    }
  };
}

const TARGET: ScanTarget = {
  deploymentId: 42, siteId: 7, siteDomain: "test.example.com",
  fileCount: 2, totalSizeMb: 0.5,
  files: [
    { path: "index.html", contentType: "text/html",  sizeBytes: 2048, objectPath: "sites/7/index.html" },
    { path: "app.js",     contentType: "application/javascript", sizeBytes: 4096, objectPath: "sites/7/app.js" },
  ],
};

describe("contentScanner — no webhook", () => {
  it("skips when webhookUrl is empty", async () => {
    const r = await makeScanner("", false)(TARGET);
    expect(r.safe).toBe(true);
    expect(r.skipped).toBe(true);
  });

  it("makes no network request when unconfigured", async () => {
    const spy = vi.spyOn(global, "fetch");
    await makeScanner("", false)(TARGET);
    expect(spy).not.toHaveBeenCalled();
    spy.mockRestore();
  });
});

describe("contentScanner — scanner approves", () => {
  it("returns safe=true, skipped=false on { safe: true } response", async () => {
    vi.spyOn(global, "fetch").mockResolvedValueOnce({ ok: true, json: async () => ({ safe: true }) } as Response);
    const r = await makeScanner("https://scanner.example", false)(TARGET);
    expect(r.safe).toBe(true);
    expect(r.skipped).toBe(false);
  });

  it("sends siteDomain and files[] in payload", async () => {
    let body: any;
    vi.spyOn(global, "fetch").mockImplementationOnce(async (_u: any, o: any) => {
      body = JSON.parse(o.body);
      return { ok: true, json: async () => ({ safe: true }) } as Response;
    });
    await makeScanner("https://scanner.example", false)(TARGET);
    expect(body.siteDomain).toBe("test.example.com");
    expect(body.files).toHaveLength(2);
    expect(body.files[0].path).toBe("index.html");
  });
});

describe("contentScanner — scanner flags content", () => {
  it("returns safe=false with reason and flaggedFiles", async () => {
    vi.spyOn(global, "fetch").mockResolvedValueOnce({
      ok: true,
      json: async () => ({ safe: false, reason: "Phishing", flaggedFiles: ["index.html"] }),
    } as Response);
    const r = await makeScanner("https://scanner.example", false)(TARGET);
    expect(r.safe).toBe(false);
    expect(r.reason).toBe("Phishing");
    expect(r.flaggedFiles).toEqual(["index.html"]);
  });
});

describe("contentScanner — fail-open (default)", () => {
  it("allows deploy when scanner throws", async () => {
    vi.spyOn(global, "fetch").mockRejectedValueOnce(new Error("ECONNREFUSED"));
    const r = await makeScanner("https://scanner.example", false)(TARGET);
    expect(r.safe).toBe(true);
  });

  it("allows deploy when scanner returns 500", async () => {
    vi.spyOn(global, "fetch").mockResolvedValueOnce({ ok: false, status: 500 } as Response);
    const r = await makeScanner("https://scanner.example", false)(TARGET);
    expect(r.safe).toBe(true);
  });
});

describe("contentScanner — fail-closed", () => {
  it("blocks deploy when scanner throws", async () => {
    vi.spyOn(global, "fetch").mockRejectedValueOnce(new Error("ECONNREFUSED"));
    const r = await makeScanner("https://scanner.example", true)(TARGET);
    expect(r.safe).toBe(false);
    expect(r.reason).toBe("Content scanner unreachable");
  });

  it("blocks deploy when scanner returns 503", async () => {
    vi.spyOn(global, "fetch").mockResolvedValueOnce({ ok: false, status: 503 } as Response);
    const r = await makeScanner("https://scanner.example", true)(TARGET);
    expect(r.safe).toBe(false);
    expect(r.reason).toContain("503");
  });
});
