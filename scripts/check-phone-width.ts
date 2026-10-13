#!/usr/bin/env bun
/**
 * Does each page fit a phone screen?
 *
 *   bun scripts/check-phone-width.ts [--width 375] [--shots <dir>] [--cookie name=value] <url> [url...]
 *
 * Loads every URL in headless Chromium emulating a phone, waits for it to
 * settle, and fails if the page is wider than the screen, naming the elements
 * that stick out past the right edge. Content that scrolls inside its own box
 * (overflow-x: auto/scroll) is allowed, as on any well-built mobile page.
 *
 * Uses Playwright's cached headless shell over the DevTools protocol, so there
 * is nothing to install. Set CHROME_BIN to use another Chromium.
 */

import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir, homedir } from "node:os";
import { join } from "node:path";

type Offender = { selector: string; right: number; text: string };
type PageResult = { url: string; viewport: number; documentWidth: number; offenders: Offender[]; error?: string };

function findChrome(): string {
  if (process.env.CHROME_BIN) return process.env.CHROME_BIN;
  const cache = join(homedir(), ".cache", "ms-playwright");
  const shells = existsSync(cache) ? readdirSync(cache).filter((d) => d.startsWith("chromium_headless_shell-")).sort().reverse() : [];
  for (const dir of shells) {
    for (const sub of readdirSync(join(cache, dir))) {
      const bin = join(cache, dir, sub, "chrome-headless-shell");
      if (existsSync(bin)) return bin;
    }
  }
  throw new Error("No headless Chromium found; set CHROME_BIN");
}

function parseArgs(argv: string[]) {
  let width = 375;
  let shots: string | null = null;
  const cookies: Array<{ name: string; value: string }> = [];
  const urls: string[] = [];
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--width") width = Number(argv[++i]);
    else if (argv[i] === "--shots") shots = argv[++i] ?? null;
    else if (argv[i] === "--cookie") {
      const [name, ...rest] = (argv[++i] ?? "").split("=");
      if (name) cookies.push({ name, value: rest.join("=") });
    }
    else urls.push(argv[i]!);
  }
  if (urls.length === 0 || !Number.isFinite(width)) {
    console.error("usage: bun scripts/check-phone-width.ts [--width 375] [--shots dir] <url> [url...]");
    process.exit(2);
  }
  return { width, shots, cookies, urls };
}

/** Runs in the page: what extends past the phone's right edge. Measured
 *  against the requested width, not innerWidth: with mobile emulation a
 *  too-wide page widens the layout viewport (phones zoom out to fit it), so
 *  the page would otherwise always "fit" itself. */
const buildMeasure = (width: number) => `(() => {
  const vw = ${width};
  const clipped = (el) => {
    for (let p = el.parentElement; p; p = p.parentElement) {
      const o = getComputedStyle(p).overflowX;
      if (o === "auto" || o === "scroll" || o === "hidden" || o === "clip") return true;
    }
    return false;
  };
  const name = (el) => el.tagName.toLowerCase() + (el.id ? "#" + el.id : "") +
    (typeof el.className === "string" && el.className.trim() ? "." + el.className.trim().split(/\\s+/).slice(0, 2).join(".") : "");
  const offenders = [];
  for (const el of document.body.querySelectorAll("*")) {
    const r = el.getBoundingClientRect();
    if (r.width === 0 || r.height === 0) continue;
    // A fixed element (an overlay, a header) stretches to whatever width the
    // page already has: a symptom of overflow, never its cause.
    if (getComputedStyle(el).position === "fixed") continue;
    if (r.right > vw + 1 && !clipped(el)) {
      offenders.push({ selector: name(el), right: Math.round(r.right), text: (el.textContent || "").trim().slice(0, 60) });
    }
  }
  // Report the outermost offenders only; their children follow from them.
  const outer = offenders.filter((o, i) => i === 0 || !offenders.slice(0, i).some((p) => o.text && p.text.includes(o.text)));
  const documentWidth = document.documentElement.scrollWidth;
  // Too wide but nothing obvious sticks out: name the widest elements anyway,
  // so the cause is never hidden behind a clipped parent.
  if (outer.length === 0 && documentWidth > vw + 1) {
    const widest = [...document.body.querySelectorAll("*")]
      .filter((el) => getComputedStyle(el).position !== "fixed")
      .map((el) => ({ el, w: Math.max(el.scrollWidth, el.getBoundingClientRect().right) }))
      .filter((x) => x.w > vw + 1)
      .sort((a, b) => b.w - a.w)
      .slice(-5);
    for (const { el, w } of widest) outer.push({ selector: name(el), right: Math.round(w), text: (el.textContent || "").trim().slice(0, 60) });
  }
  return { documentWidth, viewport: vw, offenders: outer.slice(0, 8) };
})()`;

class Cdp {
  private id = 0;
  private pending = new Map<number, (v: any) => void>();
  private waiters: Array<{ method: string; resolve: (v: any) => void }> = [];
  constructor(private ws: WebSocket) {
    ws.onmessage = (ev) => {
      const msg = JSON.parse(String(ev.data));
      if (msg.id && this.pending.has(msg.id)) {
        this.pending.get(msg.id)!(msg);
        this.pending.delete(msg.id);
      } else if (msg.method) {
        const i = this.waiters.findIndex((w) => w.method === msg.method);
        if (i >= 0) this.waiters.splice(i, 1)[0]!.resolve(msg.params);
      }
    };
  }
  send(method: string, params: object = {}, sessionId?: string): Promise<any> {
    const id = ++this.id;
    this.ws.send(JSON.stringify({ id, method, params, ...(sessionId ? { sessionId } : {}) }));
    return new Promise((resolve) => this.pending.set(id, resolve));
  }
  next(method: string, timeoutMs: number): Promise<any> {
    return new Promise((resolve) => {
      const waiter = { method, resolve };
      this.waiters.push(waiter);
      setTimeout(() => {
        const i = this.waiters.indexOf(waiter);
        if (i >= 0) { this.waiters.splice(i, 1); resolve(null); }
      }, timeoutMs);
    });
  }
}

async function checkPage(
  cdp: Cdp, url: string, width: number, shots: string | null, index: number,
  cookies: Array<{ name: string; value: string }>,
): Promise<PageResult> {
  const { result: { targetId } } = await cdp.send("Target.createTarget", { url: "about:blank" });
  const { result: { sessionId } } = await cdp.send("Target.attachToTarget", { targetId, flatten: true });
  try {
    await cdp.send("Page.enable", {}, sessionId);
    // A signed-in page needs its session cookie before the first request.
    for (const cookie of cookies) await cdp.send("Network.setCookie", { ...cookie, url }, sessionId);
    await cdp.send("Emulation.setDeviceMetricsOverride", { width, height: 800, deviceScaleFactor: 2, mobile: true }, sessionId);
    const loaded = cdp.next("Page.loadEventFired", 20_000);
    const nav = await cdp.send("Page.navigate", { url }, sessionId);
    if (nav.result?.errorText) return { url, viewport: width, documentWidth: 0, offenders: [], error: nav.result.errorText };
    await loaded;
    await Bun.sleep(1500); // let a single-page app render and fonts settle
    const evaluated = await cdp.send("Runtime.evaluate", { expression: buildMeasure(width), returnByValue: true }, sessionId);
    const value = evaluated.result?.result?.value;
    if (!value) return { url, viewport: width, documentWidth: 0, offenders: [], error: "could not measure the page" };
    if (shots) {
      const shot = await cdp.send("Page.captureScreenshot", { format: "png", captureBeyondViewport: true }, sessionId);
      writeFileSync(join(shots, `${String(index).padStart(2, "0")}.png`), Buffer.from(shot.result.data, "base64"));
    }
    return { url, viewport: value.viewport, documentWidth: value.documentWidth, offenders: value.offenders };
  } finally {
    await cdp.send("Target.closeTarget", { targetId });
  }
}

async function main() {
  const { width, shots, cookies, urls } = parseArgs(process.argv.slice(2));
  if (shots) mkdirSync(shots, { recursive: true });
  const profile = join(tmpdir(), `phone-width-${process.pid}`);
  const chrome = Bun.spawn([findChrome(), "--headless", "--no-sandbox", "--disable-gpu", "--hide-scrollbars",
    "--remote-debugging-port=0", `--user-data-dir=${profile}`, "about:blank"], { stdout: "ignore", stderr: "pipe" });
  try {
    const portFile = join(profile, "DevToolsActivePort");
    for (let i = 0; i < 100 && !existsSync(portFile); i++) await Bun.sleep(100);
    const [port, path] = readFileSync(portFile, "utf8").trim().split("\n");
    const ws = new WebSocket(`ws://127.0.0.1:${port}${path}`);
    await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
    const cdp = new Cdp(ws);

    let failed = 0;
    for (const [i, url] of urls.entries()) {
      const r = await checkPage(cdp, url, width, shots, i, cookies);
      const ok = !r.error && r.documentWidth <= r.viewport && r.offenders.length === 0;
      if (!ok) failed++;
      console.log(`${ok ? "ok  " : "FAIL"} ${url}  (${r.error ?? `page ${r.documentWidth}px on a ${r.viewport}px screen`})`);
      for (const o of r.offenders) console.log(`       sticks out to ${o.right}px: ${o.selector}  "${o.text}"`);
    }
    ws.close();
    console.log(failed === 0 ? `PASS: ${urls.length} page(s) fit a ${width}px screen` : `FAIL: ${failed} of ${urls.length} page(s) overflow a ${width}px screen`);
    process.exitCode = failed === 0 ? 0 : 1;
  } finally {
    chrome.kill();
  }
}

await main();
