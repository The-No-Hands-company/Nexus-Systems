# Ecosystem Shell and Design System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One design language every app reads from, and a shell at `app.tnhc.dev` that hosts apps inside itself the way `google.com` hosts Google's.

**Architecture:** A generator turns `tokens/nexus.tokens.json` into two CSS files — plain custom properties for any app in any language, and a Tailwind v4 `@theme` block for the React apps. Nexus-Dashboard grows the shell regions (`app-header`, `app-sidebar`, `app-content`) and mounts apps in an iframe at `/a/:appId`. Apps opt in by honouring `?embed=1` and permitting the shell as a frame ancestor.

**Tech Stack:** Bun, TypeScript, React 19, Vite, Tailwind **v4** (CSS `@theme`, no config file), vitest + jsdom, Caddy.

**Spec:** `docs/superpowers/specs/2026-08-13-ecosystem-shell-and-design-system-design.md`

## Global Constraints

- **Tailwind is v4 everywhere** (`^4.1.0` Dashboard, `^4.2.2` nexus-web, `^4.0.13` Deploy). There are no `tailwind.config.*` files and none may be added — v4 themes are CSS `@theme` blocks.
- **Both CSS outputs are generated** from `tokens/nexus.tokens.json`. Hand-editing a generated file is a defect; the drift test exists to catch it.
- **Token values are transcribed verbatim** from the schema in `docs/noname.md`. Do not invent, round, or "improve" a colour.
- **Shell lives at `app.tnhc.dev`** (Nexus-Dashboard). Do not move it to the apex.
- **`?embed=1` is the only embed signal.** No postMessage handshake in this plan.
- **Every embeddable host must end with** `frame-ancestors 'self' https://app.tnhc.dev` **and no `X-Frame-Options`.**
- Existing suites must stay green: Dashboard backend `bun run test` (17), Dashboard frontend `npx vitest run` (21), proxy+gate `bun test` (35).
- **`@testing-library/jest-dom` is not installed** in the Dashboard frontend. Use plain assertions — `expect(x).toBeTruthy()`, `.getAttribute()`, `.textContent` — not `toBeInTheDocument()`, which will fail. Available: `@testing-library/react` ^16.1.0, `react-router-dom` ^7.14.0, `vitest` ^2.1.8.
- **`isEmbedded` is duplicated in Tasks 9 and 10 on purpose.** `apps/Nexus` is a separate git repository and cannot import from the parent repo's `packages/`. This is a repo-boundary constraint, not an oversight — adjudicated before execution.
- Commit after each task. Never commit a generated file that its generator did not produce.

---

## File Structure

**New — `packages/nexus-design/`** (the design system, consumed by every app)

| File | Responsibility |
|---|---|
| `tokens/nexus.tokens.json` | Source of truth. The only file a human edits to change how Nexus looks. |
| `src/generate.ts` | Reads the JSON, writes both CSS outputs. Pure functions, no side effects except the final write. |
| `src/flatten.ts` | Turns nested token JSON into flat `--nexus-*` name/value pairs. Separated because it is the part with edge cases and the part worth testing hardest. |
| `dist/nexus-tokens.css` | Generated. Plain `:root` custom properties. For any app, any language. |
| `dist/nexus-theme.css` | Generated. Tailwind v4 `@theme` block. For the React apps. |
| `tests/flatten.test.ts` | Flattening rules. |
| `tests/generate.test.ts` | Drift: every token in the JSON reaches both outputs. |
| `package.json` | `build` script; no runtime dependencies. |

**Modified — `apps/Nexus-Dashboard/frontend/`** (becomes the shell)

| File | Responsibility |
|---|---|
| `src/index.css` | Imports the generated theme. |
| `src/shell/Shell.tsx` | The regions: header, sidebar, content. Layout only — no data fetching. |
| `src/shell/Launcher.tsx` | The app list in the sidebar. Consumes the existing `/api/apps`. |
| `src/shell/AppFrame.tsx` | The iframe. Owns `?embed=1` and the unknown-app error state. |
| `src/shell/apps.ts` | `appById()` lookup shared by Launcher and AppFrame, so the two cannot disagree about what an app is. |
| `src/App.tsx` | Adds `/a/:appId`; wraps signed-in routes in `Shell`. |

**Modified — embed contract**

| File | Responsibility |
|---|---|
| `apps/Nexus-Draw/frontend/src/*` | Honour `?embed=1`. |
| `apps/Nexus/packages/nexus-web/src/*` | Honour `?embed=1`. |
| `deploy/production/nexus-chat.Caddyfile` | Framing headers for Chat. |
| `apps/Nexus-Draw/src/server.ts` | Framing headers for Draw. |

---

### Task 1: Flatten tokens into CSS custom property pairs

The nested JSON has to become flat `--name: value` pairs. This is where the edge cases live, so it is its own unit with its own tests.

**Files:**
- Create: `packages/nexus-design/src/flatten.ts`
- Test: `packages/nexus-design/tests/flatten.test.ts`
- Create: `packages/nexus-design/package.json`

**Interfaces:**
- Produces: `flattenTokens(tokens: Record<string, unknown>, prefix?: string): Array<{ name: string; value: string }>` — `name` includes the leading `--`, values are stringified.

- [ ] **Step 1: Create the package manifest**

`packages/nexus-design/package.json`:

```json
{
  "name": "@nexus/design",
  "version": "0.1.0",
  "private": true,
  "type": "module",
  "scripts": {
    "build": "bun run src/generate.ts",
    "test": "bun test tests/"
  }
}
```

- [ ] **Step 2: Write the failing test**

`packages/nexus-design/tests/flatten.test.ts`:

```ts
import { describe, expect, test } from "bun:test";
import { flattenTokens } from "../src/flatten";

describe("flattenTokens", () => {
  test("joins nested keys with dashes and prefixes with --nexus", () => {
    const out = flattenTokens({ color: { bg: { canvas: "#090d0d" } } });
    expect(out).toEqual([{ name: "--nexus-color-bg-canvas", value: "#090d0d" }]);
  });

  test("stringifies numbers, because CSS custom properties are text", () => {
    const out = flattenTokens({ space: { "4": 16 } });
    expect(out).toEqual([{ name: "--nexus-space-4", value: "16" }]);
  });

  test("keeps every leaf, not just the first", () => {
    const out = flattenTokens({ radius: { sm: 6, md: 10, pill: 999 } });
    expect(out.map((t) => t.name)).toEqual([
      "--nexus-radius-sm",
      "--nexus-radius-md",
      "--nexus-radius-pill",
    ]);
  });

  test("preserves values that are already CSS, like rgba and cubic-bezier", () => {
    const out = flattenTokens({
      border: { subtle: "rgba(230,242,238,0.12)" },
      motion: { easing: { standard: "cubic-bezier(0.2,0,0,1)" } },
    });
    expect(out).toContainEqual({ name: "--nexus-border-subtle", value: "rgba(230,242,238,0.12)" });
    expect(out).toContainEqual({
      name: "--nexus-motion-easing-standard",
      value: "cubic-bezier(0.2,0,0,1)",
    });
  });

  test("skips metadata keys that are not design values", () => {
    // $schema and version describe the file, not how anything looks.
    const out = flattenTokens({ $schema: "https://x", version: "1.0.0", radius: { sm: 6 } });
    expect(out).toEqual([{ name: "--nexus-radius-sm", value: "6" }]);
  });

  test("returns nothing for an empty object rather than throwing", () => {
    expect(flattenTokens({})).toEqual([]);
  });
});
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cd packages/nexus-design && bun test tests/flatten.test.ts`
Expected: FAIL — cannot resolve `../src/flatten`.

- [ ] **Step 4: Implement**

`packages/nexus-design/src/flatten.ts`:

```ts
/** Keys that describe the token file itself rather than any design value. */
const METADATA_KEYS = new Set(["$schema", "version", "theme"]);

export interface TokenPair {
  name: string;
  value: string;
}

/**
 * Turn nested token JSON into flat CSS custom property pairs.
 *
 * Depth-first so output order follows the source file, which makes the
 * generated CSS reviewable against the JSON side by side.
 */
export function flattenTokens(
  tokens: Record<string, unknown>,
  prefix = "--nexus",
): TokenPair[] {
  const out: TokenPair[] = [];
  for (const [key, value] of Object.entries(tokens)) {
    if (prefix === "--nexus" && METADATA_KEYS.has(key)) continue;
    const name = `${prefix}-${key}`;
    if (value !== null && typeof value === "object" && !Array.isArray(value)) {
      out.push(...flattenTokens(value as Record<string, unknown>, name));
    } else {
      out.push({ name, value: String(value) });
    }
  }
  return out;
}
```

- [ ] **Step 5: Run it and watch it pass**

Run: `cd packages/nexus-design && bun test tests/flatten.test.ts`
Expected: PASS, 6 tests.

- [ ] **Step 6: Commit**

```bash
git add packages/nexus-design
git commit -m "feat(design): flatten nested tokens into CSS custom properties

The first half of the token pipeline. Nested JSON becomes flat --nexus-* pairs,
depth-first so the generated CSS reads in the same order as the source and the
two can be reviewed side by side.

Metadata keys are skipped: \$schema, version and theme describe the file, not
how anything looks, and emitting them as custom properties would be noise."
```

---

### Task 2: The token file and the generator

**Files:**
- Create: `packages/nexus-design/tokens/nexus.tokens.json`
- Create: `packages/nexus-design/src/generate.ts`
- Test: `packages/nexus-design/tests/generate.test.ts`

**Interfaces:**
- Consumes: `flattenTokens` from Task 1.
- Produces: `renderTokensCss(pairs: TokenPair[]): string`, `renderThemeCss(pairs: TokenPair[]): string`, and `dist/nexus-tokens.css` + `dist/nexus-theme.css` on disk.

- [ ] **Step 1: Transcribe the token file**

`packages/nexus-design/tokens/nexus.tokens.json` — copied verbatim from the schema in `docs/noname.md`, no changes:

```json
{
  "$schema": "https://nexus.systems/schemas/design-tokens.v1.json",
  "version": "1.0.0",
  "theme": { "name": "nexus-cloud-default", "mode": "dark" },
  "color": {
    "bg": { "canvas": "#090d0d", "surface": "#0f1616", "elevated": "#142020" },
    "text": { "primary": "#e6f2ee", "secondary": "#9db4ad", "muted": "#7d948d", "inverse": "#0a0f0f" },
    "accent": { "primary": "#27c9a5", "primaryHover": "#1fb592", "primaryActive": "#189f80" },
    "state": { "success": "#2ac57d", "warning": "#e8b24a", "danger": "#e15d5d", "info": "#5ca8ff" },
    "border": { "subtle": "rgba(230,242,238,0.12)", "strong": "rgba(230,242,238,0.28)" }
  },
  "typography": {
    "fontFamily": {
      "base": "IBM Plex Sans, Segoe UI, sans-serif",
      "mono": "IBM Plex Mono, ui-monospace, monospace"
    },
    "size": { "xs": 12, "sm": 14, "md": 16, "lg": 20, "xl": 28 },
    "weight": { "regular": 400, "medium": 500, "semibold": 600, "bold": 700 },
    "lineHeight": { "tight": 1.2, "normal": 1.45, "relaxed": 1.7 }
  },
  "space": { "0": 0, "1": 4, "2": 8, "3": 12, "4": 16, "5": 20, "6": 24, "8": 32, "10": 40, "12": 48 },
  "radius": { "sm": 6, "md": 10, "lg": 14, "pill": 999 },
  "shadow": {
    "sm": "0 2px 8px rgba(0,0,0,0.18)",
    "md": "0 8px 24px rgba(0,0,0,0.24)",
    "lg": "0 16px 40px rgba(0,0,0,0.32)"
  },
  "motion": {
    "duration": { "fast": 120, "normal": 180, "slow": 280 },
    "easing": { "standard": "cubic-bezier(0.2,0,0,1)", "emphasized": "cubic-bezier(0.2,0,0,0.8)" }
  },
  "zIndex": { "base": 1, "header": 50, "overlay": 100, "modal": 200, "toast": 300 }
}
```

- [ ] **Step 2: Write the failing test**

`packages/nexus-design/tests/generate.test.ts`:

```ts
import { describe, expect, test } from "bun:test";
import tokens from "../tokens/nexus.tokens.json";
import { flattenTokens } from "../src/flatten";
import { renderTokensCss, renderThemeCss } from "../src/generate";

const pairs = flattenTokens(tokens as Record<string, unknown>);

describe("generated CSS", () => {
  test("every token reaches nexus-tokens.css", () => {
    // The drift guard. Source and output disagreeing is the failure this
    // pipeline exists to prevent, so it is checked exhaustively, not sampled.
    const css = renderTokensCss(pairs);
    for (const { name, value } of pairs) {
      expect(css).toContain(`${name}: ${value};`);
    }
  });

  test("every token reaches nexus-theme.css", () => {
    const css = renderThemeCss(pairs);
    for (const { name } of pairs) {
      expect(css).toContain(name.replace("--nexus-", "--"));
    }
  });

  test("tokens css targets :root so any document picks it up", () => {
    expect(renderTokensCss(pairs)).toContain(":root {");
  });

  test("theme css uses a Tailwind v4 @theme block, not a config object", () => {
    // v4 has no tailwind.config.js. A JS preset here would be silently ignored.
    expect(renderThemeCss(pairs)).toContain("@theme {");
  });

  test("the accent colour survives the whole pipeline intact", () => {
    expect(renderTokensCss(pairs)).toContain("--nexus-color-accent-primary: #27c9a5;");
  });

  test("both outputs say they are generated", () => {
    expect(renderTokensCss(pairs)).toContain("Generated");
    expect(renderThemeCss(pairs)).toContain("Generated");
  });
});
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cd packages/nexus-design && bun test tests/generate.test.ts`
Expected: FAIL — cannot resolve `../src/generate`.

- [ ] **Step 4: Implement**

`packages/nexus-design/src/generate.ts`:

```ts
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { flattenTokens, type TokenPair } from "./flatten";
import tokens from "../tokens/nexus.tokens.json";

const BANNER = `/* Generated from tokens/nexus.tokens.json — do not edit.
   Change the JSON and run: bun run build */`;

/** Plain custom properties. Works in any document, no build step required. */
export function renderTokensCss(pairs: TokenPair[]): string {
  const body = pairs.map((p) => `  ${p.name}: ${p.value};`).join("\n");
  return `${BANNER}\n:root {\n${body}\n}\n`;
}

/**
 * Tailwind v4 theme.
 *
 * v4 reads its theme from an `@theme` block in CSS — there is no config file
 * to put a preset in. The `--nexus-` prefix is dropped here because Tailwind
 * derives utility names from the variable name: `--color-accent-primary`
 * becomes `bg-accent-primary`.
 */
export function renderThemeCss(pairs: TokenPair[]): string {
  const body = pairs
    .map((p) => `  ${p.name.replace("--nexus-", "--")}: ${p.value};`)
    .join("\n");
  return `${BANNER}\n@theme {\n${body}\n}\n`;
}

const here = dirname(fileURLToPath(import.meta.url));
const dist = join(here, "..", "dist");
const pairs = flattenTokens(tokens as Record<string, unknown>);
mkdirSync(dist, { recursive: true });
writeFileSync(join(dist, "nexus-tokens.css"), renderTokensCss(pairs));
writeFileSync(join(dist, "nexus-theme.css"), renderThemeCss(pairs));
console.log(`[nexus-design] wrote ${pairs.length} tokens to dist/`);
```

- [ ] **Step 5: Run it and watch it pass**

Run: `cd packages/nexus-design && bun test`
Expected: PASS, 12 tests total.

- [ ] **Step 6: Generate the CSS and eyeball it**

Run: `cd packages/nexus-design && bun run build`
Expected: `wrote 57 tokens to dist/`. Open `dist/nexus-tokens.css` and confirm `--nexus-color-bg-canvas: #090d0d;` is present.

- [ ] **Step 7: Commit**

```bash
git add packages/nexus-design
git commit -m "feat(design): the token file and its generator

tokens/nexus.tokens.json is transcribed verbatim from the schema in
docs/noname.md — the palette, type scale, spacing, radii, shadows, motion and
z-index that document specified and that nothing ever implemented.

Two outputs, one source: plain custom properties for any app in any language,
and a Tailwind v4 @theme block for the React apps. Not a JS preset: all three
React apps run Tailwind v4 with no config file, and v4 reads its theme from CSS,
so a preset would have been ignored silently.

The drift test checks every token reaches both outputs rather than sampling,
because source and output disagreeing is the single failure this pipeline
exists to prevent."
```

---

### Task 3: The Dashboard adopts the tokens

Proves the pipeline on a real app before any shell work depends on it.

**Files:**
- Modify: `apps/Nexus-Dashboard/frontend/src/index.css`
- Modify: `apps/Nexus-Dashboard/frontend/package.json`

**Interfaces:**
- Consumes: `packages/nexus-design/dist/nexus-theme.css`.

- [ ] **Step 1: Import the theme**

`apps/Nexus-Dashboard/frontend/src/index.css` — the import must come after Tailwind so the `@theme` block is in scope:

```css
@import "tailwindcss";
@import "../../../../packages/nexus-design/dist/nexus-theme.css";
```

- [ ] **Step 2: Make the design system build before the app does**

In `apps/Nexus-Dashboard/frontend/package.json`, change the `build` script so a fresh clone cannot build against a missing or stale `dist/`:

```json
"build": "cd ../../../packages/nexus-design && bun run build && cd - && tsc -b && vite build"
```

- [ ] **Step 3: Build and confirm a token reached the output**

```bash
cd apps/Nexus-Dashboard/frontend && npm run build
grep -c '27c9a5' dist/assets/*.css
```

Expected: at least `1`. The accent colour is in the compiled stylesheet, which means the theme was read.

- [ ] **Step 4: Confirm the existing suite still passes**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run`
Expected: 21 passed.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Dashboard/frontend
git commit -m "feat(dashboard): adopt the ecosystem design tokens

One @import, and the Dashboard's Tailwind utilities now resolve to the
ecosystem palette instead of Tailwind's defaults. The build script regenerates
the tokens first, so a fresh clone cannot compile against a stale or missing
dist/ and quietly get the wrong colours."
```

---

### Task 4: Shell regions

Layout only. No data, no iframe yet — a reviewer can reject the layout without the launcher confusing the question.

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/shell/Shell.tsx`
- Test: `apps/Nexus-Dashboard/frontend/src/shell/Shell.test.tsx`

**Interfaces:**
- Produces: `<Shell sidebar={ReactNode} children={ReactNode} />` rendering landmarks `banner`, `navigation`, `main`.

- [ ] **Step 1: Write the failing test**

`apps/Nexus-Dashboard/frontend/src/shell/Shell.test.tsx`:

```tsx
import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import Shell from "./Shell";

describe("Shell", () => {
  it("renders the three regions as landmarks", () => {
    // Landmarks rather than test ids: they are what a screen reader uses, so
    // asserting on them tests the accessibility the doctrine requires.
    render(<Shell sidebar={<nav />}>content</Shell>);
    expect(screen.getByRole("banner")).toBeTruthy();
    expect(screen.getByRole("main")).toBeTruthy();
  });

  it("puts children in the content region, not the header", () => {
    render(<Shell sidebar={<div />}>the app</Shell>);
    expect(screen.getByRole("main").textContent).toContain("the app");
    expect(screen.getByRole("banner").textContent).not.toContain("the app");
  });

  it("renders whatever sidebar it is given", () => {
    render(<Shell sidebar={<div>launcher here</div>}>x</Shell>);
    expect(screen.getByText("launcher here")).toBeTruthy();
  });

  it("shows the wordmark in the header", () => {
    render(<Shell sidebar={<div />}>x</Shell>);
    expect(screen.getByRole("banner").textContent).toContain("Nexus");
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run src/shell/Shell.test.tsx`
Expected: FAIL — cannot find `./Shell`.

- [ ] **Step 3: Implement**

`apps/Nexus-Dashboard/frontend/src/shell/Shell.tsx`:

```tsx
import type { ReactNode } from "react";

/**
 * The frame every app renders inside.
 *
 * Regions are named for the doctrine in docs/noname.md: app-header,
 * app-sidebar, app-content. The utility rail is specified there too and
 * deliberately not built — an empty named region beats an invented purpose.
 *
 * Layout only. It fetches nothing, so it can be rendered in a test without a
 * server and reasoned about without tracing data flow.
 */
export default function Shell({
  sidebar,
  children,
}: {
  sidebar: ReactNode;
  children: ReactNode;
}) {
  return (
    <div className="flex h-screen flex-col bg-bg-canvas text-text-primary">
      <header
        role="banner"
        className="flex h-14 shrink-0 items-center gap-3 border-b border-border-subtle px-4"
      >
        <span className="font-semibold tracking-tight">Nexus</span>
      </header>

      <div className="flex min-h-0 flex-1">
        <aside className="w-60 shrink-0 overflow-y-auto border-r border-border-subtle">
          {sidebar}
        </aside>

        {/* The app mounts here and nowhere else. */}
        <main role="main" className="min-w-0 flex-1 overflow-hidden">
          {children}
        </main>
      </div>
    </div>
  );
}
```

- [ ] **Step 4: Run it and watch it pass**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run src/shell/Shell.test.tsx`
Expected: PASS, 4 tests.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src/shell
git commit -m "feat(shell): the three regions

Header, sidebar and content, named for the doctrine in docs/noname.md. The
utility rail it also specifies is deliberately left unbuilt: an empty named
region is more honest than an invented purpose.

Layout only — it fetches nothing, so it renders in a test without a server.
Tests assert on landmark roles rather than test ids, because landmarks are what
a screen reader consumes and the doctrine requires keyboard and accessibility
support."
```

---

### Task 5: The launcher

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/shell/apps.ts`
- Create: `apps/Nexus-Dashboard/frontend/src/shell/Launcher.tsx`
- Test: `apps/Nexus-Dashboard/frontend/src/shell/Launcher.test.tsx`

**Interfaces:**
- Consumes: `listApps(): Promise<AppEntry[]>` from `src/api.ts`, and the existing `AppEntry` shape `{ id, name, description, url, health }`.
- Produces: `appById(apps: AppEntry[], id: string): AppEntry | undefined`, and `<Launcher apps={AppEntry[]} activeId?: string />`.

- [ ] **Step 1: Write the failing test**

`apps/Nexus-Dashboard/frontend/src/shell/Launcher.test.tsx`:

```tsx
import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import Launcher from "./Launcher";
import { appById } from "./apps";

const apps = [
  { id: "nexus-draw", name: "Draw", description: "", url: "https://draw.tnhc.dev", health: "healthy" as const },
  { id: "nexus-chat", name: "Chat", description: "", url: "https://chat.tnhc.dev", health: "offline" as const },
];

describe("Launcher", () => {
  it("lists every app", () => {
    render(<MemoryRouter><Launcher apps={apps} /></MemoryRouter>);
    expect(screen.getByText("Draw")).toBeTruthy();
    expect(screen.getByText("Chat")).toBeTruthy();
  });

  it("links a healthy app into the shell, not out to its own host", () => {
    // The whole point of the shell: clicking an app must not navigate away.
    render(<MemoryRouter><Launcher apps={apps} /></MemoryRouter>);
    expect(screen.getByRole("link", { name: /Draw/ }).getAttribute("href")).toBe("/a/nexus-draw");
  });

  it("does not link an offline app anywhere", () => {
    render(<MemoryRouter><Launcher apps={apps} /></MemoryRouter>);
    expect(screen.queryByRole("link", { name: /Chat/ })).toBeNull();
  });

  it("marks the active app for assistive tech, not just visually", () => {
    render(<MemoryRouter><Launcher apps={apps} activeId="nexus-draw" /></MemoryRouter>);
    expect(screen.getByRole("link", { name: /Draw/ }).getAttribute("aria-current")).toBe("page");
  });
});

describe("appById", () => {
  it("finds an app", () => {
    expect(appById(apps, "nexus-chat")?.name).toBe("Chat");
  });

  it("returns undefined for an unknown id rather than throwing", () => {
    expect(appById(apps, "nope")).toBeUndefined();
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run src/shell/Launcher.test.tsx`
Expected: FAIL — cannot find `./Launcher`.

- [ ] **Step 3: Implement the lookup**

`apps/Nexus-Dashboard/frontend/src/shell/apps.ts`:

```ts
import type { AppEntry } from "../api";

/**
 * Shared by the launcher and the frame so the two cannot disagree about what
 * an app is. Returns undefined rather than throwing: an unknown id arrives
 * from the URL bar, which is user input, not a programming error.
 */
export function appById(apps: AppEntry[], id: string): AppEntry | undefined {
  return apps.find((a) => a.id === id);
}
```

- [ ] **Step 4: Implement the launcher**

`apps/Nexus-Dashboard/frontend/src/shell/Launcher.tsx`:

```tsx
import { Link } from "react-router-dom";
import type { AppEntry } from "../api";

/**
 * The app list in the sidebar.
 *
 * An offline app renders as text rather than a link, for the same reason the
 * grid does it: inviting a click that goes nowhere is worse than showing the
 * app is down.
 */
export default function Launcher({
  apps,
  activeId,
}: {
  apps: AppEntry[];
  activeId?: string;
}) {
  return (
    <nav aria-label="Applications" className="p-2">
      <ul className="space-y-1">
        {apps.map((app) => (
          <li key={app.id}>
            {app.health === "healthy" ? (
              <Link
                to={`/a/${app.id}`}
                aria-current={app.id === activeId ? "page" : undefined}
                className="block rounded-md px-3 py-2 text-sm hover:bg-bg-elevated aria-[current=page]:bg-bg-elevated"
              >
                {app.name}
              </Link>
            ) : (
              <span
                className="block cursor-default rounded-md px-3 py-2 text-sm text-text-muted"
                title="This app is not running"
              >
                {app.name}
              </span>
            )}
          </li>
        ))}
      </ul>
    </nav>
  );
}
```

- [ ] **Step 5: Run it and watch it pass**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run src/shell/Launcher.test.tsx`
Expected: PASS, 6 tests.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src/shell
git commit -m "feat(shell): the app launcher

Lists the registry's apps and links them to /a/:id — into the shell rather than
out to their own hosts, which is the entire point of having a shell.

An offline app renders as plain text, not a dead link, matching what the grid
already does: inviting a click that goes nowhere is worse than showing the app
is down. appById is shared with the frame so the two cannot disagree about what
an app is, and returns undefined rather than throwing because an unknown id
arrives from the URL bar."
```

---

### Task 6: The app frame

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/shell/AppFrame.tsx`
- Test: `apps/Nexus-Dashboard/frontend/src/shell/AppFrame.test.tsx`

**Interfaces:**
- Consumes: `appById` from Task 5.
- Produces: `<AppFrame apps={AppEntry[]} appId={string} />`, and `embedUrl(url: string): string`.

- [ ] **Step 1: Write the failing test**

`apps/Nexus-Dashboard/frontend/src/shell/AppFrame.test.tsx`:

```tsx
import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import AppFrame, { embedUrl } from "./AppFrame";

const apps = [
  { id: "nexus-draw", name: "Draw", description: "", url: "https://draw.tnhc.dev", health: "healthy" as const },
];

describe("embedUrl", () => {
  it("adds the embed flag", () => {
    expect(embedUrl("https://draw.tnhc.dev")).toBe("https://draw.tnhc.dev/?embed=1");
  });

  it("keeps a query the app already had", () => {
    expect(embedUrl("https://draw.tnhc.dev/?board=7")).toBe("https://draw.tnhc.dev/?board=7&embed=1");
  });

  it("does not add it twice", () => {
    expect(embedUrl("https://draw.tnhc.dev/?embed=1")).toBe("https://draw.tnhc.dev/?embed=1");
  });
});

describe("AppFrame", () => {
  it("frames the app with the embed flag", () => {
    render(<AppFrame apps={apps} appId="nexus-draw" />);
    const frame = screen.getByTitle("Draw") as HTMLIFrameElement;
    expect(frame.src).toBe("https://draw.tnhc.dev/?embed=1");
  });

  it("titles the frame so it is reachable by assistive tech", () => {
    render(<AppFrame apps={apps} appId="nexus-draw" />);
    expect(screen.getByTitle("Draw")).toBeTruthy();
  });

  it("says so plainly when the app is unknown, instead of rendering nothing", () => {
    // A blank content area is indistinguishable from a broken shell.
    render(<AppFrame apps={apps} appId="does-not-exist" />);
    expect(screen.getByText(/not found/i)).toBeTruthy();
    expect(screen.queryByTitle("Draw")).toBeNull();
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run src/shell/AppFrame.test.tsx`
Expected: FAIL — cannot find `./AppFrame`.

- [ ] **Step 3: Implement**

`apps/Nexus-Dashboard/frontend/src/shell/AppFrame.tsx`:

```tsx
import type { AppEntry } from "../api";
import { appById } from "./apps";

/**
 * Add the embed flag to an app's URL.
 *
 * URL rather than string concatenation so an app that already carries a query
 * keeps it, and so asking twice is harmless — the frame re-renders on
 * navigation and must not accumulate parameters.
 */
export function embedUrl(url: string): string {
  const u = new URL(url);
  u.searchParams.set("embed", "1");
  return u.toString();
}

/**
 * Mounts an app inside the shell.
 *
 * No sandbox attribute: these are first-party apps that need scripts, forms,
 * storage and their own origin, so sandboxing would remove nothing an attacker
 * has and break everything the app needs. What actually constrains framing is
 * frame-ancestors on the app's own host.
 *
 * Authentication needs no work here — the session cookie is .tnhc.dev-scoped
 * and the frame is same-site, so the proxy gates and identifies the framed
 * request exactly as it does a direct one.
 */
export default function AppFrame({ apps, appId }: { apps: AppEntry[]; appId: string }) {
  const app = appById(apps, appId);

  if (!app) {
    return (
      <div className="flex h-full flex-col items-center justify-center gap-2 p-8 text-center">
        <p className="text-text-primary">App not found.</p>
        <p className="text-sm text-text-muted">
          No app is registered as <code>{appId}</code>.
        </p>
      </div>
    );
  }

  return (
    <iframe
      key={app.id}
      title={app.name}
      src={embedUrl(app.url)}
      className="h-full w-full border-0"
      allow="clipboard-read; clipboard-write; fullscreen"
    />
  );
}
```

- [ ] **Step 4: Run it and watch it pass**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run src/shell/AppFrame.test.tsx`
Expected: PASS, 6 tests.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src/shell
git commit -m "feat(shell): mount apps in a frame

embedUrl builds the app address with ?embed=1 via URL rather than string
concatenation, so an app that already carries a query keeps it and asking twice
is harmless — the frame re-renders on navigation and must not accumulate
parameters.

An unknown app id says so plainly. A blank content area is indistinguishable
from a broken shell, and the id comes from the URL bar, so it will happen.

No sandbox attribute: these are first-party apps needing scripts, forms,
storage and their own origin. Sandboxing would break what the app needs while
removing nothing an attacker has. Framing is constrained by frame-ancestors on
the app's own host, which the next task sets."
```

---

### Task 7: Wire the shell into the app

**Files:**
- Modify: `apps/Nexus-Dashboard/frontend/src/App.tsx`
- Test: `apps/Nexus-Dashboard/frontend/src/App.test.tsx`

**Interfaces:**
- Consumes: `Shell`, `Launcher`, `AppFrame`, and `listApps()` from `src/api.ts`.

- [ ] **Step 1: Read the current router**

Run: `cat apps/Nexus-Dashboard/frontend/src/App.tsx`

The routes today are `/`, `/request`, `/claim`, `/account`, `/admin`, `*`. `/request` and `/claim` are public and must **not** gain shell chrome — someone using them has no session and no apps to launch.

- [ ] **Step 2: Write the failing test**

`apps/Nexus-Dashboard/frontend/src/App.test.tsx`:

```tsx
import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";

vi.mock("./api", async () => ({
  ...(await vi.importActual<typeof import("./api")>("./api")),
  listApps: vi.fn(async () => [
    { id: "nexus-draw", name: "Draw", description: "", url: "https://draw.tnhc.dev", health: "healthy" },
  ]),
  me: vi.fn(async () => ({ username: "founder", role: "founder" })),
}));

import App from "./App";

describe("shell routing", () => {
  beforeEach(() => {
    window.history.pushState({}, "", "/a/nexus-draw");
  });

  it("mounts the requested app inside the shell", async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByTitle("Draw")).toBeTruthy());
    // Chrome and app together: the shell is present, not replaced.
    expect(screen.getByRole("banner")).toBeTruthy();
  });

  it("leaves the public claim page free of shell chrome", async () => {
    // Someone claiming an account has no session and no apps to launch;
    // wrapping that page in a launcher would be nonsense.
    window.history.pushState({}, "", "/claim");
    render(<App />);
    await waitFor(() => expect(screen.queryByRole("banner")).toBeNull());
  });
});
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run src/App.test.tsx`
Expected: FAIL — no element with title "Draw".

- [ ] **Step 4: Add the route and the shell wrapper**

In `apps/Nexus-Dashboard/frontend/src/App.tsx`, load the app list once and wrap only the signed-in routes:

```tsx
import { useEffect, useState } from "react";
import { BrowserRouter, Routes, Route, useParams } from "react-router-dom";
import { listApps, type AppEntry } from "./api";
import Shell from "./shell/Shell";
import Launcher from "./shell/Launcher";
import AppFrame from "./shell/AppFrame";

function ShellRoute({ apps }: { apps: AppEntry[] }) {
  const { appId = "" } = useParams();
  return (
    <Shell sidebar={<Launcher apps={apps} activeId={appId} />}>
      <AppFrame apps={apps} appId={appId} />
    </Shell>
  );
}
```

Add inside `<Routes>`, leaving `/request` and `/claim` exactly as they are:

```tsx
<Route path="/a/:appId" element={<ShellRoute apps={apps} />} />
```

And load the list in the component that renders `<Routes>`:

```tsx
const [apps, setApps] = useState<AppEntry[]>([]);
useEffect(() => {
  void listApps().then(setApps).catch(() => setApps([]));
}, []);
```

- [ ] **Step 5: Run it and watch it pass**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run`
Expected: PASS — the 21 existing tests plus the new ones.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src
git commit -m "feat(shell): route /a/:appId through the shell

Signed-in routes gain the shell; /request and /claim deliberately do not.
Someone claiming an account has no session and no apps to launch, so wrapping
those pages in a launcher would be nonsense — and the test asserts their
absence so a later refactor cannot quietly add it."
```

---

### Task 8: Framing headers

Without this, Chat cannot be framed at all and Draw can be framed by anyone.

**Files:**
- Modify: `deploy/production/nexus-chat.Caddyfile`
- Modify: `apps/Nexus-Draw/src/server.ts`

**Interfaces:**
- Produces: `frame-ancestors 'self' https://app.tnhc.dev` on both hosts, and no `X-Frame-Options`.

- [ ] **Step 1: Confirm the starting state**

```bash
curl -sI https://chat.tnhc.dev/ | grep -i 'x-frame-options'
curl -sI https://draw.tnhc.dev/ | grep -i 'frame-ancestors\|x-frame-options'
```

Expected: Chat prints `x-frame-options: DENY`; Draw prints nothing at all.

- [ ] **Step 2: Fix Chat**

In `deploy/production/nexus-chat.Caddyfile`, remove the `X-Frame-Options "DENY"` line from the `header` block and change the CSP's `frame-ancestors *` to:

```
frame-ancestors 'self' https://app.tnhc.dev
```

`X-Frame-Options` is obsoleted by CSP and currently contradicts it — one says deny, the other says allow anyone. Replacing both with one correct directive removes the contradiction and is stricter than either.

- [ ] **Step 3: Fix Draw**

In `apps/Nexus-Draw/src/server.ts`, add to the response headers:

```ts
// Draw sent no framing headers at all, so any site on the internet could frame
// it — a clickjacking exposure. Naming the shell explicitly closes that while
// enabling the embed.
"content-security-policy": "frame-ancestors 'self' https://app.tnhc.dev",
```

- [ ] **Step 4: Restart and verify both directions**

```bash
./deploy/production/deploy.sh bg
sleep 8
curl -sI https://chat.tnhc.dev/ | grep -i 'x-frame-options'          # expect: nothing
curl -sI https://chat.tnhc.dev/ | grep -io "frame-ancestors[^;]*"    # expect: includes app.tnhc.dev
curl -sI https://draw.tnhc.dev/ | grep -io "frame-ancestors[^;]*"    # expect: includes app.tnhc.dev
```

Both must **name the shell** and neither may allow `*`. The second half is the security half: verifying only that framing works would miss that it works for everybody.

- [ ] **Step 5: Commit**

```bash
git add deploy/production/nexus-chat.Caddyfile apps/Nexus-Draw/src/server.ts
git commit -m "fix(security): correct framing headers on the embeddable apps

Measured before changing anything: Chat sent X-Frame-Options: DENY and could
not be framed at all, while its CSP said frame-ancestors * — two directives
contradicting each other. Draw sent no framing headers whatsoever, so any site
on the internet could frame it, which is a live clickjacking exposure.

Both now name the shell and nothing else. This enables the embed and tightens
security at the same time, which is not the usual direction for a change that
makes iframes work."
```

---

### Task 9: Embed mode in Draw

Draw first: it has no sidebar of its own, so it is the smallest embed contract to satisfy.

**Files:**
- Modify: `apps/Nexus-Draw/frontend/src/App.tsx`
- Create: `apps/Nexus-Draw/frontend/src/embed.ts`
- Test: `apps/Nexus-Draw/frontend/src/embed.test.ts`

**Interfaces:**
- Produces: `isEmbedded(search?: string): boolean`.

- [ ] **Step 1: Write the failing test**

`apps/Nexus-Draw/frontend/src/embed.test.ts`:

```ts
import { describe, it, expect } from "vitest";
import { isEmbedded } from "./embed";

describe("isEmbedded", () => {
  it("is true when the shell asks for it", () => {
    expect(isEmbedded("?embed=1")).toBe(true);
  });

  it("is false normally", () => {
    expect(isEmbedded("")).toBe(false);
  });

  it("ignores other values, so ?embed=0 does not embed", () => {
    expect(isEmbedded("?embed=0")).toBe(false);
  });

  it("survives other parameters alongside it", () => {
    expect(isEmbedded("?board=7&embed=1")).toBe(true);
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cd apps/Nexus-Draw/frontend && npx vitest run src/embed.test.ts`
Expected: FAIL — cannot find `./embed`.

- [ ] **Step 3: Implement**

`apps/Nexus-Draw/frontend/src/embed.ts`:

```ts
/**
 * Whether this app is being rendered inside the ecosystem shell.
 *
 * A query parameter rather than postMessage or a header: it works identically
 * from any language and any framework, and an app that ignores it still
 * functions — just with its own chrome as well as the shell's. Degrading to
 * "slightly wrong" beats degrading to "blank".
 */
export function isEmbedded(search: string = window.location.search): boolean {
  return new URLSearchParams(search).get("embed") === "1";
}
```

- [ ] **Step 4: Suppress Draw's own chrome**

In `apps/Nexus-Draw/frontend/src/App.tsx`, import `isEmbedded` and hide the app's own top bar when true, keeping the canvas and tool palette:

```tsx
import { isEmbedded } from "./embed";
// ...
const embedded = isEmbedded();
// ...
{!embedded && <TopBar />}
```

Read the file first to find the actual top-level chrome component name; do not assume it is called `TopBar`.

- [ ] **Step 5: Run the suite and build**

```bash
cd apps/Nexus-Draw/frontend && npx vitest run && npm run build
```
Expected: all tests pass, build succeeds.

- [ ] **Step 6: Verify in the shell, signed in**

```bash
cd apps/Nexus-Dashboard/frontend && npm run build
cd ../../.. && ./deploy/production/deploy.sh bg && sleep 8
```

Then open `https://app.tnhc.dev/a/nexus-draw` in a browser, signed in. Expected: the shell header and sidebar, with Draw's canvas in the content area and **exactly one** set of chrome.

- [ ] **Step 7: Commit**

```bash
git add apps/Nexus-Draw/frontend
git commit -m "feat(draw): honour the shell's embed flag

?embed=1 suppresses Draw's own top bar so the shell's chrome is the only chrome
on screen. A query parameter rather than postMessage or a header because it
works identically from any language and any framework, and an app that ignores
it still functions — just with doubled chrome. Degrading to slightly wrong
beats degrading to blank."
```

---

### Task 10: Embed mode in Chat

**Files:**
- Create: `apps/Nexus/packages/nexus-web/src/embed.ts`
- Test: `apps/Nexus/packages/nexus-web/src/embed.test.ts`
- Modify: `apps/Nexus/packages/nexus-web/src/pages/MainLayout.tsx`

**Interfaces:**
- Produces: `isEmbedded(search?: string): boolean` — same contract as Draw's, duplicated deliberately (see commit message).

- [ ] **Step 1: Write the failing test**

`apps/Nexus/packages/nexus-web/src/embed.test.ts`:

```ts
import { describe, it, expect } from "vitest";
import { isEmbedded } from "./embed";

describe("isEmbedded", () => {
  it("is true when the shell asks for it", () => {
    expect(isEmbedded("?embed=1")).toBe(true);
  });

  it("is false normally", () => {
    expect(isEmbedded("")).toBe(false);
  });

  it("ignores other values", () => {
    expect(isEmbedded("?embed=0")).toBe(false);
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cd apps/Nexus/packages/nexus-web && npx vitest run src/embed.test.ts`
Expected: FAIL — cannot find `./embed`.

- [ ] **Step 3: Implement**

`apps/Nexus/packages/nexus-web/src/embed.ts` — identical to Draw's:

```ts
/**
 * Whether this app is being rendered inside the ecosystem shell.
 *
 * Duplicated rather than shared, and not by preference: apps/Nexus is a
 * separate git repository (a submodule), so it cannot import from the parent
 * repo's packages/ at all. Sharing four lines would mean publishing a package
 * across a repo boundary. The duplication is forced by the repo layout.
 */
export function isEmbedded(search: string = window.location.search): boolean {
  return new URLSearchParams(search).get("embed") === "1";
}
```

- [ ] **Step 4: Suppress Chat's server rail and header**

Read `apps/Nexus/packages/nexus-web/src/pages/MainLayout.tsx` and find the top-level chrome — Chat has both a server rail and a header. Hide those when embedded; keep the channel sidebar, which is Chat's content rather than its chrome.

- [ ] **Step 5: Run the suite and build**

```bash
cd apps/Nexus/packages/nexus-web && npx vitest run && npm run build
```

- [ ] **Step 6: Verify in the shell**

Rebuild the Dashboard, redeploy, and open `https://app.tnhc.dev/a/nexus-chat` signed in. Expected: one set of chrome; Chat's channels visible; messages still arrive live, which confirms the WebSocket works through the frame.

- [ ] **Step 7: Commit**

```bash
git add apps/Nexus/packages/nexus-web
git commit -m "feat(chat): honour the shell's embed flag

Suppresses Chat's server rail and header when embedded; the channel sidebar
stays, because that is Chat's content rather than its chrome.

isEmbedded is duplicated from Draw, and not by preference: apps/Nexus is a
separate git repository, so it cannot import from the parent repo's packages/.
Sharing four lines would mean publishing a package across a repo boundary."
```

---

### Task 11: Retire Cloud's portal

**Files:**
- Modify: `apps/Nexus-Cloud/src/api/handlers.ts`
- Delete: `apps/Nexus-Cloud/public/status.html`

- [ ] **Step 1: Confirm nothing else serves it**

Run: `grep -rn 'status.html\|handleDashboard' apps/Nexus-Cloud/src | grep -v test`
Expected: only the `/` and `/status` route and `handleDashboard` itself.

- [ ] **Step 2: Replace the route**

In `apps/Nexus-Cloud/src/api/handlers.ts`, change the `/` and `/status` handler to return JSON pointing at the shell, and delete `handleDashboard`:

```ts
if (request.method === "GET" && (pathname === "/" || pathname === "/status"))
  return json({
    service: "nexus-cloud",
    role: "registry, routes, orchestration, Systems API",
    // Cloud has no frontend. The console lives in the shell, which is the
    // single front door for everything a person looks at.
    console: "https://app.tnhc.dev",
  });
```

- [ ] **Step 3: Delete the portal**

```bash
git rm apps/Nexus-Cloud/public/status.html
```

- [ ] **Step 4: Verify Cloud still works headless**

```bash
cd apps/Nexus-Cloud && bun test 2>&1 | tail -3
```

Expected: 83 pass, 2 fail. **The 2 failures are pre-existing `nexus-certificate` tests** — confirm the names match and that no new failure appeared.

- [ ] **Step 5: Redeploy and confirm the API is untouched**

```bash
./deploy/production/deploy.sh bg && sleep 8
K=$(sed -n 's/^NEXUS_CLOUD_API_KEY=//p' apps/Nexus-Cloud/.env | head -1 | tr -d '\r"')
curl -s -H "X-API-Key: $K" http://127.0.0.1:8787/api/v1/routes | head -c 120
curl -s https://cloud.tnhc.dev/ | head -c 120
```

Expected: routes return as before; `cloud.tnhc.dev` returns the JSON pointer, not HTML.

- [ ] **Step 6: Commit**

```bash
git add -A apps/Nexus-Cloud
git commit -m "refactor(cloud): retire the portal; Cloud has no frontend

Cloud becomes registry, routes, orchestration and the Systems API — the control
plane, and nothing a person looks at. Its 98KB inline portal is deleted rather
than restyled: the console belongs in the shell, which is the single front door.

/ and /status now return JSON naming the shell, so anyone who lands on the host
is told where the interface actually is instead of getting a blank page."
```

---

## Self-review

**Spec coverage.** Token pipeline → Tasks 1–2. Dashboard adoption → 3. Shell regions → 4. Launcher → 5. Frame and embed contract → 6. Routing → 7. Framing headers → 8. Draw and Chat embeds → 9–10. Cloud sheds its UI → 11. Verification requirements from the spec appear as the test steps within each task rather than as a separate task, since a test written after the fact is a different and weaker thing.

**Deliberately not covered, matching the spec's out-of-scope list:** the React primitive library, deep-link route sync, moving to the apex, Cloud as orchestrator, the other 85 apps, and UI fingerprints and the Nit CI gate.

**Known gap carried forward:** Hosting gets a launcher tile through the existing registry with no work in this plan, because it serves a 2 KB placeholder with nothing to embed.

**Type consistency.** `AppEntry` is the existing type from `src/api.ts` throughout. `appById(apps, id)` has one signature, used in Tasks 5–6. `isEmbedded(search?)` has one signature in Tasks 9–10, duplicated by design and identical in both. `TokenPair` is defined in Task 1 and consumed in Task 2. `renderTokensCss` and `renderThemeCss` are named identically in the test and the implementation.
