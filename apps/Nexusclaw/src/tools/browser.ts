// Browser tool — Playwright-based web automation (Phase 3.1)
// Navigate, click, type, screenshot, DOM snapshot, manage profiles
// Playwright is a peer dependency — gracefully degrades if not installed

import type { Tool } from "../core/types.js";

// ─── Lazy Playwright loader ─────────────────────────────────────────────

let pw: any = null;
let browserInstance: any = null;
let pageInstance: any = null;

async function getPlaywright() {
  if (!pw) {
    try {
      // @ts-ignore — playwright is an optional peer dependency
      pw = await import("playwright");
    } catch {
      throw new Error(
        'Playwright is not installed. Run "npm install playwright" and "npx playwright install chromium" to enable browser tools.',
      );
    }
  }
  return pw;
}

async function ensureBrowser() {
  const { chromium } = await getPlaywright();
  if (!browserInstance || !browserInstance.isConnected()) {
    browserInstance = await chromium.launch({ headless: true });
  }
  return browserInstance;
}

async function ensurePage() {
  const browser = await ensureBrowser();
  if (!pageInstance || pageInstance.isClosed()) {
    const context = await browser.newContext({
      viewport: { width: 1280, height: 720 },
      userAgent: "AnyClaw/0.1 Browser Tool",
    });
    pageInstance = await context.newPage();
  }
  return pageInstance;
}

// ─── Tool Definitions ───────────────────────────────────────────────────

export const browserNavigateTool: Tool = {
  name: "browser_navigate",
  description: "Navigate to a URL in the headless browser. Returns the page title and a text snapshot of the visible content.",
  parameters: [
    { name: "url", type: "string", description: "URL to navigate to", required: true },
    { name: "wait_for", type: "string", description: "Wait strategy: 'load', 'domcontentloaded', 'networkidle' (default: 'load')", required: false, default: "load" },
  ],
  async execute(args) {
    const page = await ensurePage();
    const url = args.url as string;
    const waitUntil = (args.wait_for as string) || "load";

    await page.goto(url, { waitUntil, timeout: 30_000 });
    const title = await page.title();
    const text = await page.innerText("body").catch(() => "");
    const currentUrl = page.url();

    return {
      url: currentUrl,
      title,
      content: text.slice(0, 50000),
      truncated: text.length > 50000,
    };
  },
};

export const browserClickTool: Tool = {
  name: "browser_click",
  description: "Click an element on the page by CSS selector or text content.",
  parameters: [
    { name: "selector", type: "string", description: "CSS selector or text to click (e.g., 'button.submit' or 'text=Sign In')", required: true },
  ],
  async execute(args) {
    const page = await ensurePage();
    const selector = args.selector as string;

    await page.click(selector, { timeout: 10_000 });
    await page.waitForLoadState("domcontentloaded").catch(() => {});
    const title = await page.title();
    return { clicked: selector, url: page.url(), title };
  },
};

export const browserTypeTool: Tool = {
  name: "browser_type",
  description: "Type text into an input field identified by CSS selector.",
  parameters: [
    { name: "selector", type: "string", description: "CSS selector of the input element", required: true },
    { name: "text", type: "string", description: "Text to type", required: true },
    { name: "clear", type: "boolean", description: "Clear the field before typing (default: true)", required: false, default: true },
  ],
  async execute(args) {
    const page = await ensurePage();
    const selector = args.selector as string;
    const text = args.text as string;

    if (args.clear !== false) {
      await page.fill(selector, text, { timeout: 10_000 });
    } else {
      await page.type(selector, text, { timeout: 10_000 });
    }
    return { typed: text.length, selector };
  },
};

export const browserScreenshotTool: Tool = {
  name: "browser_screenshot",
  description: "Take a screenshot of the current page. Returns a base64-encoded PNG.",
  parameters: [
    { name: "selector", type: "string", description: "Optional CSS selector to screenshot a specific element", required: false },
    { name: "full_page", type: "boolean", description: "Capture the full scrollable page (default: false)", required: false, default: false },
  ],
  async execute(args) {
    const page = await ensurePage();
    const opts: any = { type: "png" };

    if (args.selector) {
      const el = await page.$(args.selector as string);
      if (!el) throw new Error(`Element not found: ${args.selector}`);
      const buf = await el.screenshot(opts);
      return { format: "png", base64: buf.toString("base64"), selector: args.selector };
    }

    if (args.full_page) opts.fullPage = true;
    const buf = await page.screenshot(opts);
    return { format: "png", base64: buf.toString("base64"), url: page.url() };
  },
};

export const browserSnapshotTool: Tool = {
  name: "browser_snapshot",
  description: "Get a structured DOM snapshot of the current page — returns an accessibility tree with element roles, names, and values. Much more compact and useful than raw HTML.",
  parameters: [
    { name: "selector", type: "string", description: "Optional CSS selector to scope the snapshot", required: false },
  ],
  async execute(args) {
    const page = await ensurePage();

    // Use accessibility tree for compact, structured output
    const snapshot = await page.accessibility.snapshot({ root: args.selector ? await page.$(args.selector as string) : undefined });

    // Also get key metadata
    const title = await page.title();
    const url = page.url();
    const meta = await page.evaluate(() => {
      // @ts-ignore — runs in browser context, document is available
      const desc = document.querySelector('meta[name="description"]');
      return { description: desc?.getAttribute("content") || "" };
    });

    return {
      url,
      title,
      description: meta.description,
      tree: snapshot,
    };
  },
};

export const browserEvalTool: Tool = {
  name: "browser_eval",
  description: "Execute JavaScript in the browser page context. Returns the expression result.",
  parameters: [
    { name: "expression", type: "string", description: "JavaScript expression to evaluate", required: true },
  ],
  async execute(args) {
    const page = await ensurePage();
    const result = await page.evaluate(args.expression as string);
    return { result };
  },
};

export const browserCloseTool: Tool = {
  name: "browser_close",
  description: "Close the browser instance and free resources.",
  parameters: [],
  async execute() {
    if (browserInstance) {
      await browserInstance.close().catch(() => {});
      browserInstance = null;
      pageInstance = null;
    }
    return { closed: true };
  },
};

// ─── All Browser Tools ──────────────────────────────────────────────────

export const browserTools: Tool[] = [
  browserNavigateTool,
  browserClickTool,
  browserTypeTool,
  browserScreenshotTool,
  browserSnapshotTool,
  browserEvalTool,
  browserCloseTool,
];
