import { describe, it, expect } from "vitest";

// ── Deployment diff logic ──────────────────────────────────────────────────────

interface FileEntry {
  filePath: string;
  contentHash: string | null;
  sizeBytes: number;
}

function computeDiff(targetFiles: FileEntry[], baseFiles: FileEntry[]) {
  const targetMap = new Map(targetFiles.map(f => [f.filePath, f]));
  const baseMap   = new Map(baseFiles.map(f => [f.filePath, f]));

  const added:     FileEntry[] = [];
  const changed:   FileEntry[] = [];
  const unchanged: FileEntry[] = [];
  const removed:   FileEntry[] = [];

  for (const [path, file] of targetMap) {
    const base = baseMap.get(path);
    if (!base) {
      added.push(file);
    } else if (file.contentHash && base.contentHash && file.contentHash !== base.contentHash) {
      changed.push(file);
    } else {
      unchanged.push(file);
    }
  }

  for (const [path, file] of baseMap) {
    if (!targetMap.has(path)) removed.push(file);
  }

  return { added, changed, removed, unchanged };
}

const f = (path: string, hash: string, size = 1024): FileEntry =>
  ({ filePath: path, contentHash: hash, sizeBytes: size });

describe("Deployment diff logic", () => {
  it("empty → empty produces no changes", () => {
    const d = computeDiff([], []);
    expect(d.added).toHaveLength(0);
    expect(d.removed).toHaveLength(0);
    expect(d.changed).toHaveLength(0);
    expect(d.unchanged).toHaveLength(0);
  });

  it("first deploy: all files are additions", () => {
    const target = [f("index.html", "hash1"), f("style.css", "hash2")];
    const d = computeDiff(target, []);
    expect(d.added).toHaveLength(2);
    expect(d.removed).toHaveLength(0);
    expect(d.changed).toHaveLength(0);
  });

  it("identical deployments: all files are unchanged", () => {
    const files = [f("index.html", "hash1"), f("app.js", "hash2")];
    const d = computeDiff(files, files);
    expect(d.unchanged).toHaveLength(2);
    expect(d.added).toHaveLength(0);
    expect(d.removed).toHaveLength(0);
    expect(d.changed).toHaveLength(0);
  });

  it("updated file detected via hash change", () => {
    const base   = [f("index.html", "old-hash"), f("app.js", "hash2")];
    const target = [f("index.html", "new-hash"), f("app.js", "hash2")];
    const d = computeDiff(target, base);
    expect(d.changed).toHaveLength(1);
    expect(d.changed[0]!.filePath).toBe("index.html");
    expect(d.unchanged).toHaveLength(1);
  });

  it("removed file detected", () => {
    const base   = [f("index.html", "hash1"), f("old-page.html", "hash2")];
    const target = [f("index.html", "hash1")];
    const d = computeDiff(target, base);
    expect(d.removed).toHaveLength(1);
    expect(d.removed[0]!.filePath).toBe("old-page.html");
    expect(d.unchanged).toHaveLength(1);
  });

  it("all change types in one diff", () => {
    const base = [
      f("index.html",   "hash-a"),
      f("old.html",     "hash-b"),
      f("style.css",    "hash-c"),
    ];
    const target = [
      f("index.html",   "hash-a"),  // unchanged
      f("style.css",    "hash-c2"), // changed
      f("new-page.html","hash-d"),  // added
      // old.html is removed
    ];
    const d = computeDiff(target, base);
    expect(d.unchanged).toHaveLength(1);
    expect(d.unchanged[0]!.filePath).toBe("index.html");
    expect(d.changed).toHaveLength(1);
    expect(d.changed[0]!.filePath).toBe("style.css");
    expect(d.added).toHaveLength(1);
    expect(d.added[0]!.filePath).toBe("new-page.html");
    expect(d.removed).toHaveLength(1);
    expect(d.removed[0]!.filePath).toBe("old.html");
  });

  it("null contentHash treats file as unchanged (legacy rows without hash)", () => {
    const base   = [f("index.html", "hash1"), { filePath: "legacy.html", contentHash: null, sizeBytes: 500 }];
    const target = [f("index.html", "hash1"), { filePath: "legacy.html", contentHash: null, sizeBytes: 500 }];
    const d = computeDiff(target, base);
    expect(d.changed).toHaveLength(0);
    expect(d.unchanged).toHaveLength(2);
  });

  it("summary net size bytes is correct", () => {
    const base   = [f("old.html", "h1", 2000)];
    const target = [f("new.html", "h2", 3000)];
    const d = computeDiff(target, base);
    const netSize = d.added.reduce((a, f) => a + f.sizeBytes, 0)
                  - d.removed.reduce((a, f) => a + f.sizeBytes, 0);
    expect(netSize).toBe(1000); // +3000 added, -2000 removed
  });
});

// ── Unlock message injection ────────────────────────────────────────────────────

function renderPasswordGate(html: string, siteId: number, domain: string, message?: string | null): string {
  return html.replace(
    "<body>",
    `<body data-site-id="${siteId}" data-domain="${domain.replace(/"/g, "&quot;")}"${
      message ? ` data-message="${message.replace(/"/g, "&quot;")}"` : ""
    }>`
  );
}

const GATE_HTML = '<html><body><p id="domain-label"></p></body></html>';

describe("Password gate unlock message injection", () => {
  it("injects data-site-id attribute", () => {
    const html = renderPasswordGate(GATE_HTML, 42, "example.com");
    expect(html).toContain('data-site-id="42"');
  });

  it("injects data-domain attribute", () => {
    const html = renderPasswordGate(GATE_HTML, 1, "mysite.example.com");
    expect(html).toContain('data-domain="mysite.example.com"');
  });

  it("injects data-message when provided", () => {
    const html = renderPasswordGate(GATE_HTML, 1, "example.com", "Members only — please log in");
    expect(html).toContain('data-message="Members only — please log in"');
  });

  it("does not add data-message when message is null", () => {
    const html = renderPasswordGate(GATE_HTML, 1, "example.com", null);
    expect(html).not.toContain("data-message");
  });

  it("does not add data-message when message is undefined", () => {
    const html = renderPasswordGate(GATE_HTML, 1, "example.com");
    expect(html).not.toContain("data-message");
  });

  it("escapes double quotes in domain", () => {
    const html = renderPasswordGate(GATE_HTML, 1, 'site"with"quotes.com');
    expect(html).toContain('data-domain="site&quot;with&quot;quotes.com"');
    expect(html).not.toContain('data-domain="site"with"');
  });

  it("escapes double quotes in message", () => {
    const html = renderPasswordGate(GATE_HTML, 1, "example.com", 'Say "hello" to enter');
    expect(html).toContain('data-message="Say &quot;hello&quot; to enter"');
  });

  it("all attributes appear on body tag", () => {
    const html = renderPasswordGate(GATE_HTML, 99, "test.com", "Custom message");
    expect(html).toContain('<body data-site-id="99" data-domain="test.com" data-message="Custom message">');
  });
});
