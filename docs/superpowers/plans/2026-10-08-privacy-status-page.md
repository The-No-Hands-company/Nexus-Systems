# Privacy Status Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish tnhc.dev/privacy with honest promise statuses and a live daily-check result, rewrite the Charter's data promises to be true today, and remove every donation ask.

**Architecture:** The ecosystem proxy serves a trimmed copy of the canary result at `status.tnhc.dev/privacy.json`. The handbook gains `privacy.md` and the rewritten Charter; tnhc.dev renders both with its existing generator, adds a small live-result box, and swaps the donation asks for one footer link.

**Tech Stack:** Bun/TypeScript (proxy, bun test), Markdown, React (CRA via craco), Python generator (`scripts/build-markdown-page.py`), bash.

**Spec:** `docs/superpowers/specs/2026-10-08-privacy-status-page-design.md`

## Global Constraints

- Public endpoint: `https://status.tnhc.dev/privacy.json`; only `GET`/`HEAD`/`OPTIONS` on `/privacy.json`; other paths 404; other methods 405.
- Response fields exactly: `status` (`pass` | `fail` | `stale`), `checkedAt` (ISO), `searched` `{databases, containers, logs, files, probes}`, `findings` (number). For `stale`: only `status` and `findings: 0`.
- Stale when the result file is missing, unreadable, unparsable, or `checkedAt` is more than 36 hours old.
- Never published: the test address, finding locations, file paths, container or database names.
- Headers: `Content-Type: application/json`, `Cache-Control: public, max-age=300`, `Access-Control-Allow-Origin: https://tnhc.dev`, plus the proxy's security headers. Ungated.
- Canary result file: `/tmp/nexus-production/privacy-canary.json` (override `PRIVACY_CANARY_RESULT` for tests). Its shape: `{"at","address","status","found":[...],"sources":{"logs","containers","databases","files","probes_answered"}}`.
- Box texts: pass → `✅ Verified <relative time>: searched N databases, N containers, N logs, N files — found nothing`; fail → `❌ The last check found a problem. We are investigating.`; stale → `Not verified recently`; fetch error → `Status unavailable`.
- Charter and privacy text: exactly as in the spec (sections 1 and 3).
- PayPal URL: `https://www.paypal.me/tnhcns`.
- Commits `type: description`, then a blank line and the session's attribution lines; push `main` fast-forward only.
- Never `pgrep -f`/`pkill`; restart the proxy only via `bash -c 'source deploy/production/deploy.sh; stop_service proxy'` then `NEXUS_PRODUCTION_START_ATTEMPTS=160 bash deploy/production/deploy.sh bg`.

---

### Task 1: Live privacy-check endpoint in the proxy

**Files:**
- Create: `deploy/production/privacy-status.ts`, `deploy/production/tests/privacy-status.test.ts`
- Modify: `deploy/production/proxy.ts` (fixed branch next to the `email-ingress.${DOMAIN}` branch, ~line 392)

**Interfaces:**
- Produces: `export async function privacyStatusBody(path?: string, now?: number): Promise<PublicPrivacyStatus>` where `type PublicPrivacyStatus = { status: "pass" | "fail" | "stale"; checkedAt?: string; searched?: { databases: number; containers: number; logs: number; files: number; probes: number }; findings: number }`. Public URL `https://status.tnhc.dev/privacy.json`.

- [ ] **Step 1: Failing tests** — `deploy/production/tests/privacy-status.test.ts`:

```ts
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
```

- [ ] **Step 2: Run, expect FAIL** — `cd deploy/production && bun test tests/privacy-status.test.ts` → module not found.

- [ ] **Step 3: Implement `deploy/production/privacy-status.ts`**

```ts
// Publishes the daily privacy check to anyone — but only the parts that are
// safe to publish. The raw result names the test address and where a finding
// was; this keeps the verdict, the time and how much was searched, nothing else.
import { readFile } from "node:fs/promises";

export type PublicPrivacyStatus = {
  status: "pass" | "fail" | "stale";
  checkedAt?: string;
  searched?: { databases: number; containers: number; logs: number; files: number; probes: number };
  findings: number;
};

const STALE_AFTER_MS = 36 * 60 * 60 * 1000;
const STALE: PublicPrivacyStatus = { status: "stale", findings: 0 };
const n = (v: unknown) => (typeof v === "number" && Number.isFinite(v) ? v : 0);

export async function privacyStatusBody(
  path: string = process.env.PRIVACY_CANARY_RESULT || "/tmp/nexus-production/privacy-canary.json",
  now: number = Date.now(),
): Promise<PublicPrivacyStatus> {
  let raw: any;
  try {
    raw = JSON.parse(await readFile(path, "utf8"));
  } catch {
    return STALE;
  }
  const at = typeof raw?.at === "string" ? Date.parse(raw.at) : NaN;
  if (!Number.isFinite(at) || now - at > STALE_AFTER_MS) return STALE;
  if (raw.status !== "pass" && raw.status !== "fail") return STALE;
  const s = raw.sources ?? {};
  return {
    status: raw.status,
    checkedAt: new Date(at).toISOString().replace(".000Z", "Z"),
    searched: { databases: n(s.databases), containers: n(s.containers), logs: n(s.logs), files: n(s.files), probes: n(s.probes_answered) },
    findings: Array.isArray(raw.found) ? raw.found.length : 0,
  };
}
```

- [ ] **Step 4: Wire into `proxy.ts`** — `import { privacyStatusBody } from "./privacy-status";` and, immediately before the `email-ingress.${DOMAIN}` branch:

```ts
    // The daily privacy check's verdict, published for anyone (tnhc.dev/privacy
    // reads it). Answered here, ungated, from a fixed file — no request input
    // reaches the filesystem.
    if (host === `status.${DOMAIN}`) {
      if (url.pathname !== "/privacy.json") return new Response("Not found", { status: 404 });
      const cors = { "access-control-allow-origin": "https://tnhc.dev", vary: "origin" };
      if (req.method === "OPTIONS") return new Response(null, { status: 204, headers: { ...cors, "access-control-allow-methods": "GET, HEAD" } });
      if (req.method !== "GET" && req.method !== "HEAD") return new Response("Method not allowed", { status: 405, headers: { allow: "GET, HEAD" } });
      const body = JSON.stringify(await privacyStatusBody());
      return new Response(req.method === "HEAD" ? null : body, {
        status: 200,
        headers: { ...cors, "content-type": "application/json", "cache-control": "public, max-age=300" },
      });
    }
```

(Security headers are added by `handleRequest`'s existing wrapper for every response.)

- [ ] **Step 5: Tests pass** — `bun test tests/privacy-status.test.ts` → all pass; full `bun test` in deploy/production → only the pre-existing `terminal-public-hop` failure (if still present).

- [ ] **Step 6: Commit + push** — `feat(proxy): publish the daily privacy check at status.tnhc.dev/privacy.json`.

- [ ] **Step 7: Deploy + verify** — restart the proxy (Global Constraints). Then: `curl -s https://status.tnhc.dev/privacy.json` → JSON with `"status":"pass"` (if the canary's last run is < 36 h old; run `bash scripts/privacy-canary.sh` first if it isn't) and no `address`/`found`; `curl -sI https://status.tnhc.dev/privacy.json | grep -i 'access-control-allow-origin: https://tnhc.dev'`.

---

### Task 2: Handbook — privacy.md and the honest Charter

**Files (repo The-No-Hands-company/handbook, clone `~/tnhc-charter-work/handbook`):**
- Create: `privacy.md`
- Modify: `charter.md` (opening promise bullets + "Your data" list), `README.md`, `scripts/check-charter.sh`

**Interfaces:** Produces `privacy.md` at the handbook root; first line `# Privacy status`. Task 3 renders it.

- [ ] **Step 1: Failing check** — extend `scripts/check-charter.sh` before its final `echo PASS`:

```bash
[ -f privacy.md ] || { echo "FAIL: privacy.md missing"; exit 1; }
head -1 privacy.md | grep -qx '# Privacy status' || { echo "FAIL: privacy.md title"; exit 1; }
grep -q 'https://tnhc.dev/privacy' charter.md || { echo "FAIL: Charter does not link the privacy status page"; exit 1; }
if grep -qi 'data of any kind' charter.md; then echo "FAIL: Charter still claims no data of any kind"; exit 1; fi
for h in '## Our promises, and where they stand' '## What we keep, and why' '## What others can see'; do
  grep -qx "$h" privacy.md || { echo "FAIL: privacy.md missing section: $h"; exit 1; }
done
```

Run `scripts/check-charter.sh` → FAIL (`privacy.md missing`).

- [ ] **Step 2: Write `privacy.md`**

```markdown
# Privacy status

What TNHC promises about your data, and exactly where each promise stands today. The live result of our daily privacy check is shown at the top of [tnhc.dev/privacy](https://tnhc.dev/privacy). This page never claims more than our code does; if you find a sentence that isn't true, please [report it](https://github.com/The-No-Hands-company/handbook/issues).

## Our promises, and where they stand

| Promise | Status | How we know |
| --- | --- | --- |
| We never store your IP address | Done | A daily automated check sends a marked test address through every public service and searches everything we run for it (live result above). A check on every code change blocks code that reads visitor addresses. |
| We keep no logs about you; technical logs live at most 24 hours | Done — a few containers keep small technical logs that are continuously overwritten rather than deleted on a timer | Our log configuration is public in our repository. |
| Your sign-in history is visible only to you and deleted after 30 days | Done. Encrypted with your own key: not done yet | Your account page shows it; our sign-in service is open source. |
| Hosted sites get page-view counts only — no visitor data | Done | Our hosting service stores page, day and a count, nothing else. |
| You can export your data in open formats | Partial — Chat only | Chat's data export. |
| Deleting your account deletes your data | Partial — an administrator can delete accounts; Chat's own account deletion blanks your account but keeps your messages | — |
| You can move your account to your own node | Not done — you can already run your own node | — |
| Your stored data is encrypted with a key only you hold | Not done | Planned as part 3 of our privacy work. |

## What we keep, and why

- **Your account details** — so you can sign in.
- **Your sign-in events, for 30 days** — so you can see if someone else got into your account.
- **Your waitlist email address, until you are invited** — so we can invite you.
- **The mail, messages and files you choose to store** — that is the service.

Nothing is kept for tracking, analytics or profiling.

## What others can see

- **Cloudflare.** All web traffic to TNHC passes through Cloudflare, which decrypts it at its edge. Cloudflare can see your IP address and what you send.
- **Resend.** Mail you send to addresses outside Nexus passes through Resend.
- **Cloudflare Email Routing.** Mail sent to you from outside Nexus passes through Cloudflare first.

We state these as known limits. Our plan to remove them is onion access first, then the [Phantom Protocol](https://tnhc.dev/phantom).
```

- [ ] **Step 3: Rewrite the Charter** — in `charter.md`, replace the three bullets under `## Our promises` and the four bullets under `### Your data` with the exact text in spec section 3 (copy it from `docs/superpowers/specs/2026-10-08-privacy-status-page-design.md` in the Nexus-Systems repo, between the ```` ```markdown ```` fences). Keep the Phantom paragraph and everything after it unchanged.

- [ ] **Step 4: README** — add under the Charter entry: `2. [Privacy status](privacy.md) — what we keep, what others can see, and where each privacy promise stands. Published at https://tnhc.dev/privacy.`

- [ ] **Step 5: Check passes** — `scripts/check-charter.sh` → PASS.

- [ ] **Step 6: Commit + push** — `docs: privacy status page; Charter data promises say what is true today`. Record the pushed SHA (Task 3 pins to it).

---

### Task 3: tnhc.dev — /privacy page with the live box

**Files (repo tnhc.dev):**
- Create: `frontend/src/lib/privacyStatus.js`, `frontend/src/lib/privacyStatus.test.js`, `frontend/src/pages/Privacy.jsx`, `frontend/src/components/site/PrivacyCheckBox.jsx`, generated `frontend/src/data/privacy.js`
- Modify: `frontend/src/App.js` (route), `scripts/webmaster-sync.sh` (generate privacy.js + regenerate charter.js), `scripts/build-sitemap.py` (`"/privacy"` in `STATIC`), `frontend/src/components/site/SiteFooterLinks.jsx` ("Privacy" link)

**Interfaces:**
- Consumes: Task 1's endpoint JSON shape; Task 2's handbook `privacy.md` + Charter at the pushed SHA.
- Produces: `export function privacyBoxState(result, now)` → `{ tone: "pass" | "fail" | "stale" | "unavailable", text: string }`.

- [ ] **Step 1: Failing test** — `frontend/src/lib/privacyStatus.test.js` (run with bun):

```js
import { describe, it, expect } from "bun:test";
import { privacyBoxState } from "./privacyStatus";

const NOW = Date.parse("2026-10-08T12:00:00Z");
describe("privacyBoxState", () => {
  it("pass shows the verified line with counts", () => {
    const s = privacyBoxState({ status: "pass", checkedAt: "2026-10-08T00:01:00Z", findings: 0,
      searched: { databases: 6, containers: 24, logs: 28, files: 5, probes: 21 } }, NOW);
    expect(s.tone).toBe("pass");
    expect(s.text).toBe("✅ Verified 12 hours ago: searched 6 databases, 24 containers, 28 logs, 5 files — found nothing");
  });
  it("fail, stale and unavailable", () => {
    expect(privacyBoxState({ status: "fail", findings: 2 }, NOW)).toEqual({ tone: "fail", text: "❌ The last check found a problem. We are investigating." });
    expect(privacyBoxState({ status: "stale", findings: 0 }, NOW)).toEqual({ tone: "stale", text: "Not verified recently" });
    expect(privacyBoxState(null, NOW)).toEqual({ tone: "unavailable", text: "Status unavailable" });
    expect(privacyBoxState({ status: "pass" }, NOW).tone).toBe("unavailable");
  });
  it("uses minutes under an hour and 'just now' under a minute", () => {
    const base = { status: "pass", findings: 0, searched: { databases: 1, containers: 1, logs: 1, files: 1, probes: 1 } };
    expect(privacyBoxState({ ...base, checkedAt: "2026-10-08T11:30:00Z" }, NOW).text).toContain("Verified 30 minutes ago");
    expect(privacyBoxState({ ...base, checkedAt: "2026-10-08T11:59:40Z" }, NOW).text).toContain("Verified just now");
  });
});
```

Run `bun test frontend/src/lib/privacyStatus.test.js` → FAIL.

- [ ] **Step 2: Implement `frontend/src/lib/privacyStatus.js`**

```js
// Turns the public privacy-check result (status.tnhc.dev/privacy.json) into
// what the page says. Anything unexpected reads as "unavailable", never as a pass.
function ago(ms) {
  const m = Math.floor(ms / 60000);
  if (m < 1) return "just now";
  if (m < 60) return `${m} minute${m === 1 ? "" : "s"} ago`;
  const h = Math.floor(m / 60);
  if (h < 48) return `${h} hour${h === 1 ? "" : "s"} ago`;
  const d = Math.floor(h / 24);
  return `${d} days ago`;
}

export function privacyBoxState(result, now = Date.now()) {
  if (!result || typeof result !== "object") return { tone: "unavailable", text: "Status unavailable" };
  if (result.status === "fail") return { tone: "fail", text: "❌ The last check found a problem. We are investigating." };
  if (result.status === "stale") return { tone: "stale", text: "Not verified recently" };
  const s = result.searched;
  const at = Date.parse(result.checkedAt);
  if (result.status !== "pass" || !s || !Number.isFinite(at)) return { tone: "unavailable", text: "Status unavailable" };
  return {
    tone: "pass",
    text: `✅ Verified ${ago(now - at)}: searched ${s.databases} databases, ${s.containers} containers, ${s.logs} logs, ${s.files} files — found nothing`,
  };
}
```

Run the test → PASS.

- [ ] **Step 3: Box + page**

`frontend/src/components/site/PrivacyCheckBox.jsx`:

```jsx
import { useEffect, useState } from "react";
import { privacyBoxState } from "@/lib/privacyStatus";

const STATUS_URL = "https://status.tnhc.dev/privacy.json";
const TONE = {
  pass: "border-acid/40 text-acid",
  fail: "border-red-400/60 text-red-300",
  stale: "border-white/20 text-white/60",
  unavailable: "border-white/20 text-white/60",
};

export default function PrivacyCheckBox() {
  const [state, setState] = useState({ tone: "unavailable", text: "Checking…" });
  useEffect(() => {
    let alive = true;
    fetch(STATUS_URL, { credentials: "omit" })
      .then((r) => (r.ok ? r.json() : null))
      .catch(() => null)
      .then((json) => { if (alive) setState(privacyBoxState(json)); });
    return () => { alive = false; };
  }, []);
  return (
    <div className={`mx-auto mt-28 max-w-3xl border px-6 py-4 font-mono text-[12px] md:px-12 ${TONE[state.tone]}`}
         data-testid="privacy-check-box" role="status">
      {state.text}
    </div>
  );
}
```

`frontend/src/pages/Privacy.jsx` — render `<PrivacyCheckBox />` above the existing `MarkdownPage` body. If `MarkdownPage` has no slot for content above the article, add an optional `before` prop to it (`export default function MarkdownPage({ doc, testId, before = null })`, rendered right after `<Header />`) and use it:

```jsx
import MarkdownPage from "@/components/site/MarkdownPage";
import PrivacyCheckBox from "@/components/site/PrivacyCheckBox";
import { PRIVACY } from "@/data/privacy";

export default function Privacy() {
  return <MarkdownPage doc={PRIVACY} testId="privacy-page" before={<PrivacyCheckBox />} />;
}
```

When `before` is given, reduce the article's top padding (`pt-32` → `pt-12`) so the box sits where the article used to start.

- [ ] **Step 4: Wiring** — `App.js`: `import Privacy from "@/pages/Privacy";` and `<Route path="/privacy" element={<Privacy />} />`; `SiteFooterLinks.jsx`: add ` · <Link to="/privacy" …>Privacy</Link>` after the Charter link (same classes); `build-sitemap.py`: add `"/privacy"` to `STATIC`; `webmaster-sync.sh`: add `frontend/src/data/privacy.js` to `GEN` and, after the charter generation:

```bash
python3 scripts/build-markdown-page.py --repo "$HANDBOOK" --file privacy.md \
    --out frontend/src/data/privacy.js --export PRIVACY \
    --source https://github.com/The-No-Hands-company/handbook/blob/main/privacy.md \
    || { echo "FAIL: privacy generation" >&2; exit 1; }
```

- [ ] **Step 5: Generate + build** — `bash scripts/webmaster-sync.sh`; confirm `frontend/src/data/privacy.js` and `charter.js` carry Task 2's handbook SHA; `python3 scripts/build-sitemap.py`; `cd frontend && npx craco build` → success; `bun test frontend/src/lib/privacyStatus.test.js` and `python3 -m pytest tests -q` → pass.

- [ ] **Step 6: Commit + push** — `feat: privacy status page with the live daily-check result` (include regenerated data files and sitemap).

- [ ] **Step 7: Live verify** (Cloudflare Pages deploys on push; poll ≤10 min): `https://tnhc.dev/privacy` → 200; the live bundle contains `privacy-check-box` and `Privacy status`; the bundle for `/charter` no longer contains `data of any kind`. Open https://tnhc.dev/privacy in a headless fetch is not enough for the box (client-side) — verify CORS instead: `curl -s -o /dev/null -w '%{http_code}' -H 'Origin: https://tnhc.dev' https://status.tnhc.dev/privacy.json` → 200 with the ACAO header.

---

### Task 4: tnhc.dev — remove every donation ask, add the footer link

**Files (repo tnhc.dev):**
- Modify: `frontend/src/pages/Landing.jsx` (remove `<Support />` + import), `frontend/src/components/site/Waitlist.jsx` (remove `<Donate />` + import), `frontend/src/pages/Apps.jsx` (remove the `apps-donate-button` and `apps-footer-donate` links), `frontend/src/components/site/SiteFooterLinks.jsx` (add Donate link)
- Delete: `frontend/src/components/site/Support.jsx`, `frontend/src/components/site/Donate.jsx` (only if nothing imports them afterwards)

- [ ] **Step 1: Failing check** — `scripts/check-no-donation-ask.sh`:

```bash
#!/usr/bin/env bash
# The Charter says we never ask for donations: the only mention allowed is the
# plain footer link. Dated blog posts are history and stay.
set -euo pipefail
cd "$(dirname "$0")/.."
hits=$(grep -rniE 'support the build|paypal' frontend/src --include=*.jsx --include=*.js \
  | grep -v 'frontend/src/data/' | grep -v 'components/site/SiteFooterLinks.jsx' || true)
[ -z "$hits" ] && echo PASS || { echo "FAIL: donation asks remain:"; echo "$hits"; exit 1; }
```

Run → FAIL listing Support.jsx, Donate.jsx, Apps.jsx.

- [ ] **Step 2: Remove the asks** as listed in Files; in `SiteFooterLinks.jsx` add, after the Privacy link: ` · <a href="https://www.paypal.me/tnhcns" target="_blank" rel="noopener noreferrer" className="transition-colors hover:text-acid">Donate</a>`. Keep each page's other content intact (only the ask elements and their now-unused imports go). Remove now-unused test ids from `frontend/src/constants/testIds/*` only if they are referenced nowhere.

- [ ] **Step 3: Check passes** — `bash scripts/check-no-donation-ask.sh` → PASS; `cd frontend && npx craco build` → success.

- [ ] **Step 4: Commit + push** — `feat: no donation asks — one quiet footer link, as the Charter promises`.

- [ ] **Step 5: Live verify** — after the Pages deploy, the live main bundle contains none of `donate-paypal-button`, `apps-donate-button`, `apps-footer-donate` (the removed asks' test ids), and the landing page has no Support section (`grep -c 'support-section\|Support the build' ` on the bundle counts only the dated blog post's title, which stays).

---

### Task 5: Final verification

- [ ] `https://status.tnhc.dev/privacy.json` returns the safe fields with `"status":"pass"`.
- [ ] `https://tnhc.dev/privacy` and `https://tnhc.dev/charter` → 200; Charter page links to /privacy; footer shows Charter · Privacy · Founder · Donate.
- [ ] Handbook `scripts/check-charter.sh` → PASS; tnhc.dev `scripts/check-no-donation-ask.sh` → PASS.
- [ ] `bash scripts/privacy-canary.sh` (Nexus-Systems) → pass.
- [ ] Report to the user with links.
