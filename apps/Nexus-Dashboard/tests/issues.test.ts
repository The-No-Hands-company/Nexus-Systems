import { describe, it, expect } from "bun:test";
import { validateReport, renderBody, fileIssue } from "../src/issues";

describe("validateReport", () => {
  it("accepts a minimal report", () => {
    const r = validateReport({ title: "Deploy fails", body: "It 500s on upload." });
    expect("report" in r).toBe(true);
    if ("report" in r) {
      expect(r.report.title).toBe("Deploy fails");
      expect(r.report.app).toBeUndefined();
    }
  });

  it("requires a title and a description", () => {
    expect(validateReport({ body: "x" })).toEqual({ error: "title is required" });
    expect(validateReport({ title: "x" })).toEqual({ error: "description is required" });
    // Whitespace is not content.
    expect(validateReport({ title: "   ", body: "x" })).toEqual({ error: "title is required" });
  });

  it("rejects non-objects rather than throwing", () => {
    // The endpoint is reachable by anything holding a session, so it is fed
    // whatever an attacker likes — including null and arrays.
    expect("error" in validateReport(null)).toBe(true);
    expect("error" in validateReport("a string")).toBe(true);
    expect("error" in validateReport(42)).toBe(true);
  });

  it("enforces length limits", () => {
    const long = "x".repeat(200);
    expect("error" in validateReport({ title: long, body: "ok" })).toBe(true);
    expect("error" in validateReport({ title: "ok", body: "x".repeat(9000) })).toBe(true);
  });

  it("truncates context rather than rejecting it", () => {
    // Context is a convenience, not something a reporter should lose their
    // whole report over.
    const r = validateReport({ title: "t", body: "b", app: "x".repeat(200), url: "y".repeat(500) });
    expect("report" in r).toBe(true);
    if ("report" in r) {
      expect(r.report.app!.length).toBe(60);
      expect(r.report.url!.length).toBe(300);
    }
  });
});

describe("renderBody", () => {
  it("puts the reporter's words first and unedited", () => {
    const body = renderBody({ title: "t", body: "The upload button does nothing." }, "user-123");
    expect(body.startsWith("The upload button does nothing.")).toBe(true);
  });

  it("marks the context as system-added", () => {
    const body = renderBody({ title: "t", body: "b", app: "hosting", url: "https://x/y" }, "user-123");
    expect(body).toContain("Filed from the Nexus dashboard");
    expect(body).toContain("hosting");
    expect(body).toContain("https://x/y");
  });

  it("identifies the reporter by opaque subject, never an email address", () => {
    // A public issue tracker must not publish anyone's address.
    const body = renderBody({ title: "t", body: "b" }, "auth|abc123");
    expect(body).toContain("auth|abc123");
    expect(body).not.toMatch(/[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}/);
  });
});

describe("fileIssue", () => {
  /**
   * Every test here stubs fetch, without exception.
   *
   * The first version passed `undefined` as the token to mean "not
   * configured". That is not what it means: an omitted argument falls through
   * to the default, which reads process.env.NEXUS_ISSUES_TOKEN — and bun
   * auto-loads apps/Nexus-Dashboard/.env, so the moment a real token existed
   * on this machine the suite started making live calls to GitHub. With a
   * valid token it would have filed a real issue titled "t" on every run.
   *
   * An empty string is the honest way to say "no token", and stubbing fetch
   * globally means a mistake like that cannot reach the network again.
   */
  const noNetwork = () =>
    ((async () => {
      throw new Error("test attempted a real network call");
    }) as unknown as typeof fetch);

  it("reports unconfigured rather than failing obscurely", async () => {
    const original = globalThis.fetch;
    globalThis.fetch = noNetwork();
    try {
      const r = await fileIssue({ title: "t", body: "b" }, "user-1", "");
      expect(r.ok).toBe(false);
      if (!r.ok) {
        expect(r.status).toBe(503);
        expect(r.error).toContain("not configured");
      }
    } finally {
      globalThis.fetch = original;
    }
  });

  it("does not leak GitHub's message on an auth failure", async () => {
    // A 401 from GitHub means our token is wrong, which is an operator
    // problem. Relaying GitHub's wording can name the repo and scopes.
    const original = globalThis.fetch;
    globalThis.fetch = (async () =>
      new Response(JSON.stringify({ message: "Bad credentials for repo secret/private" }), {
        status: 401,
      })) as unknown as typeof fetch;
    try {
      const r = await fileIssue({ title: "t", body: "b" }, "user-1", "tok");
      expect(r.ok).toBe(false);
      if (!r.ok) {
        expect(r.status).toBe(503);
        expect(r.error).not.toContain("secret/private");
        expect(r.error).not.toContain("Bad credentials");
      }
    } finally {
      globalThis.fetch = original;
    }
  });

  it("returns the issue number and url on success", async () => {
    const original = globalThis.fetch;
    globalThis.fetch = (async () =>
      new Response(JSON.stringify({ number: 42, html_url: "https://github.com/o/r/issues/42" }), {
        status: 201,
      })) as unknown as typeof fetch;
    try {
      const r = await fileIssue({ title: "t", body: "b" }, "user-1", "tok");
      expect(r.ok).toBe(true);
      if (r.ok) {
        expect(r.number).toBe(42);
        expect(r.url).toContain("/issues/42");
      }
    } finally {
      globalThis.fetch = original;
    }
  });

  it("survives a network failure as a retryable error", async () => {
    const original = globalThis.fetch;
    globalThis.fetch = (async () => {
      throw new Error("ECONNREFUSED");
    }) as unknown as typeof fetch;
    try {
      const r = await fileIssue({ title: "t", body: "b" }, "user-1", "tok");
      expect(r.ok).toBe(false);
      if (!r.ok) expect(r.status).toBe(504);
    } finally {
      globalThis.fetch = original;
    }
  });
});
