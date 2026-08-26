# Nexus Design Foundation and Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a token layer shaped as a theme family (4 themes × 2 densities, contrast-gated), a component kit sufficient for the shell, and a dashboard rebuilt around a command-palette apps drawer instead of an 86-tile grid.

**Architecture:** `packages/nexus-design` stops emitting one palette and starts emitting theme- and density-scoped custom-property blocks, with Void and Balanced also written to bare `:root` so an unstamped document renders the default. Consumers are unchanged: each app's `index.css` already aliases Tailwind's zinc scale onto `--nexus-color-*`, so a theme swap is an attribute change and no component is touched. The shell then rebuilds against a small kit of primitives that read tokens only.

**Tech Stack:** Bun (runtime + test runner), TypeScript, **React 18.3**, Tailwind v4, `bun:test` for the design package, `vitest 2.1` + `@testing-library/react 16` + `jsdom 25` for component tests in the Dashboard frontend, `clsx` + `tailwind-merge` for class composition.

## Global Constraints

- Void `#030303` is the default theme; Balanced is the default density. A document with no `data-nexus-theme` / `data-nexus-density` renders those.
- Every theme declares **every** token name. A missing token fails the build — it must never fall through to another theme's value.
- Components in `packages/nexus-design/src/components/ui/` read tokens only. A hex literal in that directory is a defect and a test enforces it.
- Contrast gate: **4.5:1** for `text.primary` and `text.secondary` against every background; **3:1** for `text.muted`, state colours, and accent-as-UI. Below that, the build fails.
- The theme *switcher*, cross-origin propagation and flash-of-wrong-theme are **out of scope** — a later spec. Density and pinned-apps preferences persist in `localStorage` on `app.tnhc.dev` only.
- Offline apps appear in the drawer, dimmed and unactivatable. Never hidden, never clickable.
- Every widget has four distinct states: loading, empty, error, content. An API failure must never render as empty.
- No horizontal page scroll at 320px, 768px or 1280px.
- `bun test` is the runner for `packages/nexus-design`; the Dashboard frontend uses `vitest`. Do not mix them.
- **`@testing-library/jest-dom` is NOT installed.** Assert with `toBeTruthy()`,
  `toBeNull()` and attribute reads — `toBeInTheDocument()` and friends do not
  exist here and will fail at runtime, not at typecheck.
- React is **18.3**, not 19. Do not use `use()`, the `ref`-as-prop form, or any
  other 19-only API.

---

## File Structure

**`packages/nexus-design/`**
- `tokens/nexus.tokens.json` — MODIFIED. Keeps `typography.fontFamily/weight/lineHeight`, `radius`, `shadow`, `motion`, `zIndex`. Loses `color`, `space`, `typography.size` — those move to theme/density files.
- `tokens/themes/{void,abyss,petrol,slate}.json` — CREATE. Colour only.
- `tokens/density/{balanced,compact}.json` — CREATE. `space`, `typography.size`, `control`, `widget`.
- `src/flatten.ts` — unchanged. Already generic over any nested object.
- `src/generate.ts` — MODIFIED. Emits scoped blocks; runs the contrast gate before writing.
- `src/contrast.ts` — CREATE. Colour parsing, WCAG ratio, theme validation.
- `src/components/ui/{button,card,input}.tsx` — MODIFIED. Hex literals replaced with token classes.
- `src/components/ui/{pill,kbd,avatar,overlay,empty-state,skeleton}.tsx` — CREATE.
- `tests/` — `contrast.test.ts`, `themes.test.ts`, `components.test.ts` CREATE; `generate.test.ts` MODIFIED.

**`apps/Nexus-Dashboard/frontend/src/`**
- `shell/Shell.tsx` — MODIFIED. Top bar, rail, density attribute.
- `shell/AppsDrawer.tsx` — CREATE. Palette + browse.
- `shell/useDensity.ts` — CREATE. Preference read/write/apply.
- `shell/HealthStrip.tsx` — CREATE.
- `pages/Home.tsx` — MODIFIED. Widgets replace `<Grid/>`.
- `pages/widgets/{Today,Unread,Activity,Pinned}.tsx` — CREATE.
- `pages/widgets/WidgetShell.tsx` — CREATE. The four-state wrapper every widget uses.
- `pages/Grid.tsx` — retained, but reached only from the drawer's browse view.

---

## Task 1: Split the tokens into a theme family

**Files:**
- Modify: `packages/nexus-design/tokens/nexus.tokens.json`
- Create: `packages/nexus-design/tokens/themes/void.json`, `abyss.json`, `petrol.json`, `slate.json`
- Create: `packages/nexus-design/tokens/density/balanced.json`, `compact.json`
- Create: `packages/nexus-design/tests/themes.test.ts`

**Interfaces:**
- Produces: theme JSON files whose top-level key is `color`; density JSON files whose top-level keys are `space`, `typography`, `control`, `widget`.
- Consumes: `flattenTokens(obj, prefix)` from `src/flatten.ts` — unchanged signature `(tokens: Record<string, unknown>, prefix?: string) => TokenPair[]` where `TokenPair = { name: string; value: string }`.

- [ ] **Step 1: Write the failing test**

Create `packages/nexus-design/tests/themes.test.ts`:

```ts
import { describe, expect, test } from "bun:test";
import void_ from "../tokens/themes/void.json";
import abyss from "../tokens/themes/abyss.json";
import petrol from "../tokens/themes/petrol.json";
import slate from "../tokens/themes/slate.json";
import balanced from "../tokens/density/balanced.json";
import compact from "../tokens/density/compact.json";
import { flattenTokens } from "../src/flatten";

const THEMES = { void: void_, abyss, petrol, slate } as Record<string, Record<string, unknown>>;
const DENSITIES = { balanced, compact } as Record<string, Record<string, unknown>>;

function names(obj: Record<string, unknown>): string[] {
  return flattenTokens(obj).map((p) => p.name).sort();
}

describe("theme family", () => {
  test("every theme declares exactly the same token names", () => {
    const reference = names(THEMES.void!);
    expect(reference.length).toBeGreaterThan(0);
    for (const [id, theme] of Object.entries(THEMES)) {
      expect({ id, names: names(theme) }).toEqual({ id, names: reference });
    }
  });

  test("every density declares exactly the same token names", () => {
    const reference = names(DENSITIES.balanced!);
    for (const [id, density] of Object.entries(DENSITIES)) {
      expect({ id, names: names(density) }).toEqual({ id, names: reference });
    }
  });

  test("themes carry colour only, densities carry no colour", () => {
    for (const theme of Object.values(THEMES)) {
      expect(Object.keys(theme)).toEqual(["color"]);
    }
    for (const density of Object.values(DENSITIES)) {
      expect(Object.keys(density)).not.toContain("color");
    }
  });

  test("the base token file no longer carries anything a theme or density owns", () => {
    const base = require("../tokens/nexus.tokens.json") as Record<string, unknown>;
    expect(base).not.toHaveProperty("color");
    expect(base).not.toHaveProperty("space");
    expect((base.typography as Record<string, unknown>)).not.toHaveProperty("size");
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd packages/nexus-design && bun test tests/themes.test.ts`
Expected: FAIL — `Cannot find module '../tokens/themes/void.json'`.

- [ ] **Step 3: Create the four theme files**

`tokens/themes/void.json`:

```json
{
  "color": {
    "bg": { "canvas": "#030303", "surface": "#0A0A0A", "elevated": "#111111", "raised": "#171717" },
    "text": { "primary": "#FFFFFF", "secondary": "#A0A0A0", "muted": "#6E6E6E", "inverse": "#030303" },
    "accent": { "primary": "#CCFF00", "hover": "#B8E600", "active": "#A3CC00" },
    "state": { "success": "#2AC57D", "warning": "#E8B24A", "danger": "#E15D5D", "info": "#5CA8FF" },
    "border": { "subtle": "rgba(255,255,255,0.10)", "strong": "rgba(255,255,255,0.20)" }
  }
}
```

`tokens/themes/abyss.json`:

```json
{
  "color": {
    "bg": { "canvas": "#04100E", "surface": "#08201C", "elevated": "#0C2B26", "raised": "#113630" },
    "text": { "primary": "#F0FBF8", "secondary": "#9DB8B1", "muted": "#6E8A83", "inverse": "#04100E" },
    "accent": { "primary": "#D4FF3D", "hover": "#C2F520", "active": "#AEE000" },
    "state": { "success": "#4ADE80", "warning": "#F0BE5A", "danger": "#F07171", "info": "#7DD3FC" },
    "border": { "subtle": "rgba(140,255,225,0.12)", "strong": "rgba(140,255,225,0.24)" }
  }
}
```

`tokens/themes/petrol.json`:

```json
{
  "color": {
    "bg": { "canvas": "#07131A", "surface": "#0C1F28", "elevated": "#112B36", "raised": "#163643" },
    "text": { "primary": "#F2F8FA", "secondary": "#A0B4BD", "muted": "#71858F", "inverse": "#07131A" },
    "accent": { "primary": "#CCFF00", "hover": "#B8E600", "active": "#A3CC00" },
    "state": { "success": "#34D399", "warning": "#EBB454", "danger": "#EF6E6E", "info": "#60A5FA" },
    "border": { "subtle": "rgba(150,220,255,0.12)", "strong": "rgba(150,220,255,0.24)" }
  }
}
```

`tokens/themes/slate.json`:

```json
{
  "color": {
    "bg": { "canvas": "#080D16", "surface": "#0D1421", "elevated": "#141D2E", "raised": "#1A2539" },
    "text": { "primary": "#F1F4FA", "secondary": "#A3AFC4", "muted": "#74809A", "inverse": "#080D16" },
    "accent": { "primary": "#C2F000", "hover": "#AFDA00", "active": "#9BC300" },
    "state": { "success": "#34D399", "warning": "#EBB454", "danger": "#EF6E6E", "info": "#60A5FA" },
    "border": { "subtle": "rgba(160,190,255,0.12)", "strong": "rgba(160,190,255,0.24)" }
  }
}
```

- [ ] **Step 4: Create the two density files**

`tokens/density/balanced.json`:

```json
{
  "space": { "1": 4, "2": 8, "3": 12, "4": 16, "5": 20, "6": 24, "7": 32, "8": 48 },
  "typography": { "size": { "xs": 11, "sm": 13, "base": 14, "lg": 16, "xl": 20, "2xl": 28 } },
  "control": { "height": 32 },
  "widget": { "padding": 16 }
}
```

`tokens/density/compact.json`:

```json
{
  "space": { "1": 2, "2": 4, "3": 8, "4": 12, "5": 16, "6": 20, "7": 24, "8": 32 },
  "typography": { "size": { "xs": 10, "sm": 12, "base": 13, "lg": 15, "xl": 18, "2xl": 24 } },
  "control": { "height": 26 },
  "widget": { "padding": 12 }
}
```

- [ ] **Step 5: Strip the moved groups out of the base token file**

Edit `tokens/nexus.tokens.json`: delete the `color` key entirely, delete the `space` key entirely, and delete `typography.size`. Keep `$schema`, `version`, `theme`, `typography.fontFamily`, `typography.weight`, `typography.lineHeight`, `radius`, `shadow`, `motion`, `zIndex`.

- [ ] **Step 6: Run the test to verify it passes**

Run: `cd packages/nexus-design && bun test tests/themes.test.ts`
Expected: PASS, 4 tests.

- [ ] **Step 7: Commit**

```bash
git add packages/nexus-design/tokens packages/nexus-design/tests/themes.test.ts
git commit -m "feat(design): split tokens into a theme family and two density scales"
```

---

## Task 2: Contrast validator

**Files:**
- Create: `packages/nexus-design/src/contrast.ts`
- Create: `packages/nexus-design/tests/contrast.test.ts`

**Interfaces:**
- Produces:
  - `parseColor(value: string): { r: number; g: number; b: number; a: number }` — accepts `#rgb`, `#rrggbb`, `#rrggbbaa`, `rgba(r,g,b,a)`.
  - `composite(fg: RGBA, bg: RGBA): RGBA` — alpha-composites `fg` over an opaque `bg`.
  - `contrastRatio(fg: string, bg: string): number` — WCAG 2.1 ratio, 1–21, compositing `fg` over `bg` when `fg` is translucent.
  - `validateTheme(themeId: string, colors: Record<string, string>): Finding[]` where `Finding = { themeId: string; pair: string; ratio: number; required: number; ok: boolean }`.
  - `type RGBA = { r: number; g: number; b: number; a: number }`.
- Consumes: nothing from earlier tasks.

- [ ] **Step 1: Write the failing test**

Create `packages/nexus-design/tests/contrast.test.ts`:

```ts
import { describe, expect, test } from "bun:test";
import { parseColor, contrastRatio, validateTheme } from "../src/contrast";

describe("parseColor", () => {
  test("reads hex and rgba", () => {
    expect(parseColor("#FFFFFF")).toEqual({ r: 255, g: 255, b: 255, a: 1 });
    expect(parseColor("#030303")).toEqual({ r: 3, g: 3, b: 3, a: 1 });
    expect(parseColor("rgba(255,255,255,0.10)")).toEqual({ r: 255, g: 255, b: 255, a: 0.1 });
  });

  test("rejects anything it does not understand rather than guessing", () => {
    expect(() => parseColor("chartreuse")).toThrow(/unsupported colour/i);
  });
});

describe("contrastRatio", () => {
  test("black on white is the documented maximum", () => {
    expect(contrastRatio("#000000", "#FFFFFF")).toBeCloseTo(21, 1);
  });

  test("a colour against itself is 1", () => {
    expect(contrastRatio("#3A7BD5", "#3A7BD5")).toBeCloseTo(1, 5);
  });

  test("order does not matter", () => {
    expect(contrastRatio("#FFFFFF", "#030303")).toBeCloseTo(contrastRatio("#030303", "#FFFFFF"), 5);
  });

  test("a translucent foreground is composited over the background first", () => {
    // 10% white over near-black is a dark grey, not white — so the ratio
    // against that same near-black must be tiny, not ~20.
    expect(contrastRatio("rgba(255,255,255,0.10)", "#030303")).toBeLessThan(2);
  });
});

describe("validateTheme", () => {
  const good = {
    "bg.canvas": "#030303", "bg.surface": "#0A0A0A", "bg.elevated": "#111111",
    "text.primary": "#FFFFFF", "text.secondary": "#A0A0A0", "text.muted": "#6E6E6E",
    "accent.primary": "#CCFF00",
    "state.success": "#2AC57D", "state.warning": "#E8B24A",
    "state.danger": "#E15D5D", "state.info": "#5CA8FF",
  };

  test("passes a palette that meets AA", () => {
    expect(validateTheme("void", good).filter((f) => !f.ok)).toEqual([]);
  });

  // The gate must be able to fail. A validator that cannot reject anything is
  // the exact failure mode this repository has been burned by.
  test("rejects text that does not meet 4.5:1", () => {
    const bad = { ...good, "text.secondary": "#151515" };
    const failures = validateTheme("void", bad).filter((f) => !f.ok);
    expect(failures.length).toBeGreaterThan(0);
    expect(failures.map((f) => f.pair)).toContain("text.secondary on bg.canvas");
  });

  test("rejects a state colour that does not meet 3:1", () => {
    const bad = { ...good, "state.success": "#0B2A1C" };
    const failures = validateTheme("void", bad).filter((f) => !f.ok);
    expect(failures.map((f) => f.pair)).toContain("state.success on bg.surface");
  });

  test("reports the ratio and the requirement, not just a boolean", () => {
    const findings = validateTheme("void", good);
    const one = findings.find((f) => f.pair === "text.primary on bg.canvas")!;
    expect(one.required).toBe(4.5);
    expect(one.ratio).toBeGreaterThan(4.5);
    expect(one.themeId).toBe("void");
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd packages/nexus-design && bun test tests/contrast.test.ts`
Expected: FAIL — `Cannot find module '../src/contrast'`.

- [ ] **Step 3: Implement the validator**

Create `packages/nexus-design/src/contrast.ts`:

```ts
/**
 * WCAG 2.1 contrast, used as a build gate.
 *
 * Only Void is exercised on real screens in this increment, so the other three
 * themes ship as data. This is what makes them verified data rather than three
 * untried guesses.
 */

export type RGBA = { r: number; g: number; b: number; a: number };

const HEX = /^#([0-9a-f]{3}|[0-9a-f]{6}|[0-9a-f]{8})$/i;
const RGBA_FN = /^rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)$/i;

export function parseColor(value: string): RGBA {
  const v = value.trim();

  const hex = v.match(HEX);
  if (hex) {
    let h = hex[1]!;
    if (h.length === 3) h = h.split("").map((c) => c + c).join("");
    const int = parseInt(h.slice(0, 6), 16);
    const a = h.length === 8 ? parseInt(h.slice(6, 8), 16) / 255 : 1;
    return { r: (int >> 16) & 255, g: (int >> 8) & 255, b: int & 255, a };
  }

  const fn = v.match(RGBA_FN);
  if (fn) {
    return {
      r: Number(fn[1]), g: Number(fn[2]), b: Number(fn[3]),
      a: fn[4] === undefined ? 1 : Number(fn[4]),
    };
  }

  // Named colours and modern syntaxes are deliberately unsupported: guessing at
  // a value here would make the gate lie about a colour it cannot actually read.
  throw new Error(`unsupported colour: ${value}`);
}

/** Alpha-composite `fg` over an assumed-opaque `bg`. */
export function composite(fg: RGBA, bg: RGBA): RGBA {
  const a = fg.a;
  return {
    r: fg.r * a + bg.r * (1 - a),
    g: fg.g * a + bg.g * (1 - a),
    b: fg.b * a + bg.b * (1 - a),
    a: 1,
  };
}

function channel(c: number): number {
  const s = c / 255;
  return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4;
}

function luminance(c: RGBA): number {
  return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
}

export function contrastRatio(fg: string, bg: string): number {
  const back = parseColor(bg);
  const front = parseColor(fg);
  const solid = front.a < 1 ? composite(front, back) : front;
  const l1 = luminance(solid);
  const l2 = luminance(back);
  const [hi, lo] = l1 >= l2 ? [l1, l2] : [l2, l1];
  return (hi + 0.05) / (lo + 0.05);
}

export type Finding = {
  themeId: string;
  pair: string;
  ratio: number;
  required: number;
  ok: boolean;
};

const BACKGROUNDS = ["bg.canvas", "bg.surface", "bg.elevated"];

/**
 * Body text must clear 4.5:1. Muted text, state colours and the accent are
 * treated as non-text UI at 3:1 — muted is for de-emphasised and disabled
 * copy, and the state colours appear as pills and dots rather than prose.
 */
const RULES: Array<{ token: string; required: number }> = [
  { token: "text.primary", required: 4.5 },
  { token: "text.secondary", required: 4.5 },
  { token: "text.muted", required: 3 },
  { token: "accent.primary", required: 3 },
  { token: "state.success", required: 3 },
  { token: "state.warning", required: 3 },
  { token: "state.danger", required: 3 },
  { token: "state.info", required: 3 },
];

export function validateTheme(themeId: string, colors: Record<string, string>): Finding[] {
  const findings: Finding[] = [];
  for (const { token, required } of RULES) {
    const fg = colors[token];
    if (fg === undefined) throw new Error(`${themeId}: missing token ${token}`);
    for (const bgToken of BACKGROUNDS) {
      const bg = colors[bgToken];
      if (bg === undefined) throw new Error(`${themeId}: missing token ${bgToken}`);
      const ratio = contrastRatio(fg, bg);
      findings.push({
        themeId,
        pair: `${token} on ${bgToken}`,
        ratio: Math.round(ratio * 100) / 100,
        required,
        ok: ratio >= required,
      });
    }
  }
  return findings;
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cd packages/nexus-design && bun test tests/contrast.test.ts`
Expected: PASS, 9 tests.

- [ ] **Step 5: Commit**

```bash
git add packages/nexus-design/src/contrast.ts packages/nexus-design/tests/contrast.test.ts
git commit -m "feat(design): WCAG contrast validator that can actually fail"
```

---

## Task 3: Emit scoped CSS and gate the build on contrast

**Files:**
- Modify: `packages/nexus-design/src/generate.ts`
- Modify: `packages/nexus-design/tests/generate.test.ts`

**Interfaces:**
- Consumes: `flattenTokens` (Task 1 unchanged), `validateTheme` and `Finding` from `src/contrast.ts` (Task 2).
- Produces:
  - `renderScopedCss(base: TokenPair[], themes: Record<string, TokenPair[]>, densities: Record<string, TokenPair[]>, defaults: { theme: string; density: string }): string`
  - `renderContrastReport(findings: Finding[]): string`
  - `dist/nexus-tokens.css` gains `[data-nexus-theme]` and `[data-nexus-density]` blocks; `dist/contrast-report.md` is new.

- [ ] **Step 1: Write the failing test**

Append to `packages/nexus-design/tests/generate.test.ts`:

```ts
import { renderScopedCss } from "../src/generate";

describe("scoped css", () => {
  const base = [{ name: "--nexus-radius-md", value: "10" }];
  const themes = {
    void: [{ name: "--nexus-color-bg-canvas", value: "#030303" }],
    abyss: [{ name: "--nexus-color-bg-canvas", value: "#04100E" }],
  };
  const densities = {
    balanced: [{ name: "--nexus-space-4", value: "16" }],
    compact: [{ name: "--nexus-space-4", value: "12" }],
  };
  const css = renderScopedCss(base, themes, densities, { theme: "void", density: "balanced" });

  test("the default theme is also written to bare :root", () => {
    // A document with no data-nexus-theme must render Void, not nothing.
    expect(css).toMatch(/:root,\s*\[data-nexus-theme="void"\]\s*\{[^}]*#030303/);
  });

  test("the default density is also written to bare :root", () => {
    expect(css).toMatch(/:root,\s*\[data-nexus-density="balanced"\]\s*\{[^}]*16px/);
  });

  test("non-default themes are scoped only to their attribute", () => {
    expect(css).toMatch(/\[data-nexus-theme="abyss"\]\s*\{[^}]*#04100E/);
    expect(css).not.toMatch(/:root,\s*\[data-nexus-theme="abyss"\]/);
  });

  test("base tokens are emitted once, unscoped", () => {
    expect(css.match(/--nexus-radius-md/g)).toHaveLength(1);
  });

  test("density values still receive their unit", () => {
    expect(css).toContain("--nexus-space-4: 12px;");
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd packages/nexus-design && bun test tests/generate.test.ts`
Expected: FAIL — `renderScopedCss is not a function`.

- [ ] **Step 3: Implement scoped rendering and the gate**

In `src/generate.ts`, keep `renderTokensCss` and `renderThemeCss` exported and unchanged. Add above the bottom script block:

```ts
import { validateTheme, type Finding } from "./contrast";

/** One selector's worth of declarations. */
function block(selector: string, pairs: TokenPair[]): string {
  const body = withUnits(pairs).map((p) => `  ${p.name}: ${p.value};`).join("\n");
  return `${selector} {\n${body}\n}\n`;
}

/**
 * Theme- and density-scoped custom properties.
 *
 * The defaults are emitted on bare `:root` *as well as* their own attribute, so
 * a document that never sets data-nexus-theme still renders Void rather than a
 * page with no colours at all.
 */
export function renderScopedCss(
  base: TokenPair[],
  themes: Record<string, TokenPair[]>,
  densities: Record<string, TokenPair[]>,
  defaults: { theme: string; density: string },
): string {
  const out = [BANNER, "", block(":root", base)];

  for (const [id, pairs] of Object.entries(themes)) {
    const selector = id === defaults.theme
      ? `:root, [data-nexus-theme="${id}"]`
      : `[data-nexus-theme="${id}"]`;
    out.push(block(selector, pairs));
  }
  for (const [id, pairs] of Object.entries(densities)) {
    const selector = id === defaults.density
      ? `:root, [data-nexus-density="${id}"]`
      : `[data-nexus-density="${id}"]`;
    out.push(block(selector, pairs));
  }
  return out.join("\n");
}

export function renderContrastReport(findings: Finding[]): string {
  const rows = findings
    .map((f) => `| ${f.themeId} | ${f.pair} | ${f.ratio.toFixed(2)} | ${f.required} | ${f.ok ? "pass" : "**FAIL**"} |`)
    .join("\n");
  return [
    "# Contrast report",
    "",
    "Generated by `bun run build`. Body text requires 4.5:1; muted text, state",
    "colours and the accent are non-text UI at 3:1.",
    "",
    "| Theme | Pair | Ratio | Required | |",
    "|---|---|---|---|---|",
    rows,
    "",
  ].join("\n");
}
```

Replace the bottom script block entirely with:

```ts
const here = dirname(fileURLToPath(import.meta.url));
const dist = join(here, "..", "dist");
const tokensDir = join(here, "..", "tokens");

const THEME_IDS = ["void", "abyss", "petrol", "slate"] as const;
const DENSITY_IDS = ["balanced", "compact"] as const;

async function loadJson(path: string): Promise<Record<string, unknown>> {
  return JSON.parse(await Bun.file(path).text()) as Record<string, unknown>;
}

const base = flattenTokens(tokens as Record<string, unknown>);

const themeJson: Record<string, Record<string, unknown>> = {};
const themes: Record<string, TokenPair[]> = {};
for (const id of THEME_IDS) {
  themeJson[id] = await loadJson(join(tokensDir, "themes", `${id}.json`));
  themes[id] = flattenTokens(themeJson[id]!);
}

const densities: Record<string, TokenPair[]> = {};
for (const id of DENSITY_IDS) {
  densities[id] = flattenTokens(await loadJson(join(tokensDir, "density", `${id}.json`)));
}

// Contrast gate. Runs before anything is written, so a failing palette cannot
// leave stale-but-valid CSS on disk looking like a successful build.
const findings: Finding[] = [];
for (const id of THEME_IDS) {
  const colors = Object.fromEntries(
    flattenTokens(themeJson[id]!)
      .map((p) => [p.name.replace("--nexus-color-", "").replace(/-/g, "."), p.value]),
  ) as Record<string, string>;
  findings.push(...validateTheme(id, colors));
}
const failures = findings.filter((f) => !f.ok);

mkdirSync(dist, { recursive: true });
writeFileSync(join(dist, "contrast-report.md"), renderContrastReport(findings));

if (failures.length > 0) {
  for (const f of failures) {
    console.error(`[nexus-design] ${f.themeId}: ${f.pair} is ${f.ratio} (needs ${f.required})`);
  }
  throw new Error(`contrast gate failed: ${failures.length} pair(s) below AA`);
}

writeFileSync(
  join(dist, "nexus-tokens.css"),
  renderScopedCss(base, themes, densities, { theme: "void", density: "balanced" }),
);
writeFileSync(join(dist, "nexus-theme.css"), renderThemeCss([...base, ...themes.void!, ...densities.balanced!]));
console.log(`[nexus-design] wrote ${THEME_IDS.length} themes, ${DENSITY_IDS.length} densities, ${findings.length} contrast pairs`);
```

Note the flatten-name-to-dotted-token mapping: `--nexus-color-bg-canvas` becomes `bg.canvas`, which is what `validateTheme` expects.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cd packages/nexus-design && bun test tests/`
Expected: PASS. Then run the build: `bun run build`
Expected: writes `dist/contrast-report.md` and `dist/nexus-tokens.css`, prints the counts line, exit 0.

- [ ] **Step 5: Verify the gate fails on a bad palette**

Temporarily set `text.secondary` in `tokens/themes/void.json` to `#151515`, run `bun run build`, and confirm it exits non-zero naming the pair. Then restore the file.

Run: `bun run build; echo "exit=$?"`
Expected: `exit=1` and a `[nexus-design] void: text.secondary on bg.canvas is …` line.

- [ ] **Step 6: Commit**

```bash
git add packages/nexus-design/src/generate.ts packages/nexus-design/tests/generate.test.ts packages/nexus-design/dist
git commit -m "feat(design): emit theme- and density-scoped tokens behind a contrast gate"
```

---

## Task 4: Make the package able to build and test React, and de-hex the existing components

**Files:**
- Modify: `packages/nexus-design/package.json`
- Create: `packages/nexus-design/tests/components.test.ts`
- Modify: `packages/nexus-design/src/components/ui/button.tsx`, `card.tsx`, `input.tsx`

**Interfaces:**
- Produces: a `no hex literals` guard the rest of the kit must satisfy; `cn(...inputs: ClassValue[]): string` extracted to `src/components/ui/cn.ts` so every component shares one copy.
- Consumes: nothing from earlier tasks.

**Context the implementer needs:** `button.tsx` currently hard-codes `#ccff00`, `#b8e600`, `#a3cc00`, `#111111`, `#1a1a1a`, `#0a0a0a`. The package declares **no dependencies** at all despite importing `react`, `clsx` and `tailwind-merge` — these components have only ever compiled inside Dashboard, which supplies them. Both facts have to be fixed before any new component can be added.

- [ ] **Step 1: Write the failing test**

Create `packages/nexus-design/tests/components.test.ts`:

```ts
import { describe, expect, test } from "bun:test";
import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

const UI = join(import.meta.dir, "..", "src", "components", "ui");

function sources(): Array<{ file: string; text: string }> {
  return readdirSync(UI)
    .filter((f) => f.endsWith(".tsx") || f.endsWith(".ts"))
    .map((f) => ({ file: f, text: readFileSync(join(UI, f), "utf8") }));
}

describe("the component kit reads tokens only", () => {
  test("no file contains a hex colour literal", () => {
    const offenders = sources()
      .map(({ file, text }) => ({ file, hits: text.match(/#[0-9a-fA-F]{3,8}\b/g) ?? [] }))
      .filter(({ hits }) => hits.length > 0);
    expect(offenders).toEqual([]);
  });

  test("no file contains a raw rgba() literal", () => {
    const offenders = sources()
      .map(({ file, text }) => ({ file, hits: text.match(/rgba?\(/g) ?? [] }))
      .filter(({ hits }) => hits.length > 0);
    expect(offenders).toEqual([]);
  });

  test("there is exactly one copy of cn()", () => {
    const defs = sources().filter(({ text }) => /function cn\(/.test(text));
    expect(defs.map((d) => d.file)).toEqual(["cn.ts"]);
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd packages/nexus-design && bun test tests/components.test.ts`
Expected: FAIL — offenders lists `button.tsx` with six hex hits, and `cn()` is defined in three files.

- [ ] **Step 3: Declare the dependencies the components already import**

Replace `packages/nexus-design/package.json` with:

```json
{
  "name": "@nexus/design",
  "version": "0.1.0",
  "private": true,
  "type": "module",
  "main": "src/index.ts",
  "scripts": {
    "build": "bun run src/generate.ts",
    "vendor": "bun run build && bun run scripts/vendor.ts",
    "test": "bun test tests/"
  },
  "dependencies": {
    "clsx": "^2.1.1",
    "tailwind-merge": "^2.5.4"
  },
  "peerDependencies": {
    "react": ">=18"
  }
}
```

Then run `bun install` inside `packages/nexus-design`.

- [ ] **Step 4: Extract cn() and convert the three components to token classes**

Create `src/components/ui/cn.ts`:

```ts
import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

/** The one class-composition helper. Duplicating it per component is how three
 *  copies drifted apart before this file existed. */
export function cn(...inputs: ClassValue[]): string {
  return twMerge(clsx(inputs));
}
```

Rewrite `src/components/ui/button.tsx`:

```tsx
import React from "react";
import { cn } from "./cn";

export interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: "primary" | "secondary" | "outline" | "ghost";
  size?: "sm" | "md" | "lg";
}

/**
 * Colours come from Tailwind utilities that resolve to ecosystem tokens through
 * each app's index.css alias. A hex literal here would pin the button to one
 * theme and silently ignore the other three.
 */
export const Button = React.forwardRef<HTMLButtonElement, ButtonProps>(
  ({ className, variant = "primary", size = "md", ...props }, ref) => {
    const variants = {
      primary: "bg-accent text-accent-foreground hover:bg-accent-hover active:bg-accent-active",
      secondary: "bg-zinc-700 text-zinc-100 hover:bg-zinc-600",
      outline: "border border-zinc-600 text-zinc-100 hover:bg-zinc-800",
      ghost: "text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800",
    };
    const sizes = {
      sm: "px-3 py-1.5 text-xs",
      md: "px-4 py-2 text-sm",
      lg: "px-6 py-3 text-base",
    };
    return (
      <button
        ref={ref}
        className={cn(
          "inline-flex items-center justify-center rounded-md font-medium transition-colors",
          "focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent",
          "disabled:pointer-events-none disabled:opacity-50",
          variants[variant],
          sizes[size],
          className,
        )}
        {...props}
      />
    );
  },
);
Button.displayName = "Button";
```

Apply the same treatment to `card.tsx` and `input.tsx`: delete their local `cn`, import it from `./cn`, and replace every hex or `rgba(` literal with a `zinc-*` / `accent*` utility.

- [ ] **Step 5: Add the accent utilities to the Dashboard alias**

In `apps/Nexus-Dashboard/frontend/src/index.css`, inside the existing `@theme` block, add:

```css
  --color-accent: var(--nexus-color-accent-primary);
  --color-accent-hover: var(--nexus-color-accent-hover);
  --color-accent-active: var(--nexus-color-accent-active);
  --color-accent-foreground: var(--nexus-color-text-inverse);
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `cd packages/nexus-design && bun test tests/`
Expected: PASS, all files including the three component guards.

- [ ] **Step 7: Commit**

```bash
git add packages/nexus-design/package.json packages/nexus-design/src/components packages/nexus-design/tests/components.test.ts apps/Nexus-Dashboard/frontend/src/index.css
git commit -m "fix(design): declare the kit's real dependencies and remove every hex literal"
```

---

## Task 5: Pill, Kbd and Avatar

**Files:**
- Create: `packages/nexus-design/src/components/ui/pill.tsx`, `kbd.tsx`, `avatar.tsx`
- Create: `packages/nexus-design/src/index.ts`

**Interfaces:**
- Consumes: `cn` from `./cn` (Task 4).
- Produces:
  - `Pill({ tone?: "neutral" | "success" | "warning" | "danger" | "info", children, className })`
  - `Kbd({ children, className })`
  - `Avatar({ label: string, className })` — renders up to two initials derived from `label`.
  - `src/index.ts` re-exporting every component, so consumers import from one place.

- [ ] **Step 1: Write the failing test**

Append to `packages/nexus-design/tests/components.test.ts`:

```ts
import { initials } from "../src/components/ui/avatar";

describe("Avatar initials", () => {
  test("takes the first letter of the first two words", () => {
    expect(initials("Eric Hakansson")).toBe("EH");
  });
  test("falls back to the first two characters of a single word", () => {
    expect(initials("nexus")).toBe("NE");
  });
  test("survives empty and whitespace input rather than rendering nothing", () => {
    expect(initials("")).toBe("?");
    expect(initials("   ")).toBe("?");
  });
});

describe("kit barrel", () => {
  test("every component file is re-exported from src/index.ts", async () => {
    const barrel = readFileSync(join(UI, "..", "..", "index.ts"), "utf8");
    for (const { file } of sources()) {
      if (file === "cn.ts") continue;
      const stem = file.replace(/\.tsx?$/, "");
      expect(barrel).toContain(`./components/ui/${stem}`);
    }
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd packages/nexus-design && bun test tests/components.test.ts`
Expected: FAIL — `Cannot find module '../src/components/ui/avatar'`.

- [ ] **Step 3: Implement the three components**

`src/components/ui/pill.tsx`:

```tsx
import React from "react";
import { cn } from "./cn";

export type PillTone = "neutral" | "success" | "warning" | "danger" | "info";

export interface PillProps extends React.HTMLAttributes<HTMLSpanElement> {
  tone?: PillTone;
}

/** A status chip. The dot carries the state colour so the label stays legible
 *  at 3:1 rather than tinting text that has to meet 4.5:1. */
export const Pill = React.forwardRef<HTMLSpanElement, PillProps>(
  ({ className, tone = "neutral", children, ...props }, ref) => {
    const dot = {
      neutral: "bg-zinc-500",
      success: "bg-state-success",
      warning: "bg-state-warning",
      danger: "bg-state-danger",
      info: "bg-state-info",
    }[tone];
    return (
      <span
        ref={ref}
        className={cn(
          "inline-flex items-center gap-1.5 rounded-full border border-zinc-600",
          "px-2 py-0.5 text-xs text-zinc-300",
          className,
        )}
        {...props}
      >
        <span aria-hidden="true" className={cn("h-1.5 w-1.5 rounded-full", dot)} />
        {children}
      </span>
    );
  },
);
Pill.displayName = "Pill";
```

`src/components/ui/kbd.tsx`:

```tsx
import React from "react";
import { cn } from "./cn";

/** A keyboard hint. Presentational only — the shortcut itself is bound by
 *  whatever renders this. */
export function Kbd({ className, children, ...props }: React.HTMLAttributes<HTMLElement>) {
  return (
    <kbd
      className={cn(
        "inline-flex min-w-5 items-center justify-center rounded border border-zinc-600",
        "bg-zinc-800 px-1.5 py-0.5 font-mono text-xs text-zinc-400",
        className,
      )}
      {...props}
    >
      {children}
    </kbd>
  );
}
```

`src/components/ui/avatar.tsx`:

```tsx
import React from "react";
import { cn } from "./cn";

/** Up to two initials, and never an empty circle. */
export function initials(label: string): string {
  const words = label.trim().split(/\s+/).filter(Boolean);
  if (words.length === 0) return "?";
  if (words.length === 1) return words[0]!.slice(0, 2).toUpperCase();
  return (words[0]![0]! + words[1]![0]!).toUpperCase();
}

export interface AvatarProps extends React.HTMLAttributes<HTMLSpanElement> {
  label: string;
}

export function Avatar({ label, className, ...props }: AvatarProps) {
  return (
    <span
      role="img"
      aria-label={label}
      className={cn(
        "inline-flex h-7 w-7 items-center justify-center rounded-full",
        "border border-zinc-600 font-mono text-xs text-zinc-300",
        className,
      )}
      {...props}
    >
      {initials(label)}
    </span>
  );
}
```

`src/index.ts`:

```ts
export { cn } from "./components/ui/cn";
export { Button, type ButtonProps } from "./components/ui/button";
export { Card } from "./components/ui/card";
export { Input } from "./components/ui/input";
export { Pill, type PillProps, type PillTone } from "./components/ui/pill";
export { Kbd } from "./components/ui/kbd";
export { Avatar, initials, type AvatarProps } from "./components/ui/avatar";
```

- [ ] **Step 4: Add the state utilities to the Dashboard alias**

In `apps/Nexus-Dashboard/frontend/src/index.css`, inside `@theme`, add:

```css
  --color-state-success: var(--nexus-color-state-success);
  --color-state-warning: var(--nexus-color-state-warning);
  --color-state-danger: var(--nexus-color-state-danger);
  --color-state-info: var(--nexus-color-state-info);
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cd packages/nexus-design && bun test tests/`
Expected: PASS, including the no-hex guard against the three new files.

- [ ] **Step 6: Commit**

```bash
git add packages/nexus-design/src apps/Nexus-Dashboard/frontend/src/index.css packages/nexus-design/tests/components.test.ts
git commit -m "feat(design): add Pill, Kbd and Avatar, and a barrel export"
```

---

## Task 6: Overlay

**Files:**
- Create: `packages/nexus-design/src/components/ui/overlay.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/overlay.test.tsx`
- Modify: `packages/nexus-design/src/index.ts`

**Interfaces:**
- Consumes: `cn` (Task 4).
- Produces: `Overlay({ open: boolean, onClose: () => void, label: string, children })` — a focus-trapped modal surface. Escape closes; focus returns to whatever was focused before it opened.

**Why the test lives in the Dashboard:** `packages/nexus-design` runs `bun test` with no DOM. The Dashboard frontend already has vitest + jsdom + `@testing-library/react` configured, so rendering tests go there. Do not add jsdom to the design package.

- [ ] **Step 1: Write the failing test**

Create `apps/Nexus-Dashboard/frontend/src/shell/__tests__/overlay.test.tsx`:

```tsx
import { describe, it, expect, vi } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { Overlay } from "../../../../../../packages/nexus-design/src/components/ui/overlay";

describe("Overlay", () => {
  it("renders nothing when closed", () => {
    render(<Overlay open={false} onClose={() => {}} label="Apps"><button>inside</button></Overlay>);
    expect(screen.queryByRole("dialog")).toBeNull();
  });

  it("exposes itself as a labelled modal dialog", () => {
    render(<Overlay open onClose={() => {}} label="Apps"><button>inside</button></Overlay>);
    const dialog = screen.getByRole("dialog");
    expect(dialog.getAttribute("aria-modal")).toBe("true");
    expect(dialog.getAttribute("aria-label")).toBe("Apps");
  });

  it("closes on Escape", () => {
    const onClose = vi.fn();
    render(<Overlay open onClose={onClose} label="Apps"><button>inside</button></Overlay>);
    fireEvent.keyDown(document, { key: "Escape" });
    expect(onClose).toHaveBeenCalledOnce();
  });

  it("moves focus inside when it opens", () => {
    render(<Overlay open onClose={() => {}} label="Apps"><button>inside</button></Overlay>);
    expect(document.activeElement).toBe(screen.getByRole("button", { name: "inside" }));
  });

  it("returns focus to the trigger when it closes", () => {
    const trigger = document.createElement("button");
    document.body.appendChild(trigger);
    trigger.focus();
    const { rerender } = render(
      <Overlay open onClose={() => {}} label="Apps"><button>inside</button></Overlay>,
    );
    rerender(<Overlay open={false} onClose={() => {}} label="Apps"><button>inside</button></Overlay>);
    expect(document.activeElement).toBe(trigger);
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- overlay`
Expected: FAIL — cannot resolve `overlay`.

- [ ] **Step 3: Implement Overlay**

Create `packages/nexus-design/src/components/ui/overlay.tsx`:

```tsx
import React from "react";
import { cn } from "./cn";

export interface OverlayProps {
  open: boolean;
  onClose: () => void;
  /** Accessible name. A modal with no name is unusable with a screen reader. */
  label: string;
  children: React.ReactNode;
  className?: string;
}

/**
 * The first modal surface in the system.
 *
 * Focus moves in on open and back to the previously focused element on close —
 * without the second half, dismissing the apps drawer drops focus to the body
 * and keyboard navigation restarts from the top of the page.
 */
export function Overlay({ open, onClose, label, children, className }: OverlayProps) {
  const panel = React.useRef<HTMLDivElement>(null);
  const restoreTo = React.useRef<HTMLElement | null>(null);

  React.useEffect(() => {
    if (!open) return;
    restoreTo.current = document.activeElement as HTMLElement | null;

    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") { e.stopPropagation(); onClose(); }
    };
    document.addEventListener("keydown", onKey);

    const first = panel.current?.querySelector<HTMLElement>(
      'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])',
    );
    (first ?? panel.current)?.focus();

    return () => {
      document.removeEventListener("keydown", onKey);
      restoreTo.current?.focus();
    };
  }, [open, onClose]);

  if (!open) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-start justify-center p-4 sm:p-8">
      <div
        aria-hidden="true"
        onClick={onClose}
        className="absolute inset-0 bg-zinc-900/70 backdrop-blur-sm"
      />
      <div
        ref={panel}
        role="dialog"
        aria-modal="true"
        aria-label={label}
        tabIndex={-1}
        className={cn(
          "relative w-full max-w-xl rounded-lg border border-zinc-600",
          "bg-zinc-800 shadow-lg outline-none",
          className,
        )}
      >
        {children}
      </div>
    </div>
  );
}
```

Add to `src/index.ts`:

```ts
export { Overlay, type OverlayProps } from "./components/ui/overlay";
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- overlay`
Expected: PASS, 5 tests.

- [ ] **Step 5: Commit**

```bash
git add packages/nexus-design/src apps/Nexus-Dashboard/frontend/src/shell/__tests__/overlay.test.tsx
git commit -m "feat(design): focus-trapped Overlay that restores focus on close"
```

---

## Task 7: EmptyState and Skeleton

**Files:**
- Create: `packages/nexus-design/src/components/ui/empty-state.tsx`, `skeleton.tsx`
- Modify: `packages/nexus-design/src/index.ts`

**Interfaces:**
- Consumes: `cn` (Task 4).
- Produces:
  - `EmptyState({ title: string, hint?: string, action?: React.ReactNode })`
  - `Skeleton({ lines?: number, className?: string })` — `lines` defaults to 3.

- [ ] **Step 1: Write the failing test**

Create `apps/Nexus-Dashboard/frontend/src/shell/__tests__/states.test.tsx`:

```tsx
import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import { EmptyState } from "../../../../../../packages/nexus-design/src/components/ui/empty-state";
import { Skeleton } from "../../../../../../packages/nexus-design/src/components/ui/skeleton";

describe("EmptyState", () => {
  it("shows the title and the hint", () => {
    render(<EmptyState title="Nothing today" hint="Events you create will appear here" />);
    expect(screen.getByText("Nothing today")).toBeTruthy();
    expect(screen.getByText("Events you create will appear here")).toBeTruthy();
  });
});

describe("Skeleton", () => {
  it("announces itself as busy so loading is not read as empty", () => {
    render(<Skeleton />);
    expect(screen.getByRole("status").getAttribute("aria-busy")).toBe("true");
  });

  it("renders the requested number of lines", () => {
    const { container } = render(<Skeleton lines={5} />);
    expect(container.querySelectorAll("[data-skeleton-line]")).toHaveLength(5);
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- states`
Expected: FAIL — cannot resolve `empty-state`.

- [ ] **Step 3: Implement both**

`src/components/ui/empty-state.tsx`:

```tsx
import React from "react";
import { cn } from "./cn";

export interface EmptyStateProps {
  title: string;
  hint?: string;
  action?: React.ReactNode;
  className?: string;
}

/** "Nothing here" — distinct from "we could not ask", which is an error state. */
export function EmptyState({ title, hint, action, className }: EmptyStateProps) {
  return (
    <div className={cn("flex flex-col items-start gap-1 py-6 text-sm", className)}>
      <p className="text-zinc-300">{title}</p>
      {hint ? <p className="text-zinc-500">{hint}</p> : null}
      {action ? <div className="pt-2">{action}</div> : null}
    </div>
  );
}
```

`src/components/ui/skeleton.tsx`:

```tsx
import React from "react";
import { cn } from "./cn";

export interface SkeletonProps {
  lines?: number;
  className?: string;
}

/** Loading, announced as such. A silent placeholder is indistinguishable from
 *  an empty result to anyone using a screen reader. */
export function Skeleton({ lines = 3, className }: SkeletonProps) {
  return (
    <div role="status" aria-busy="true" aria-live="polite" className={cn("flex flex-col gap-2 py-2", className)}>
      <span className="sr-only">Loading</span>
      {Array.from({ length: lines }, (_, i) => (
        <span
          key={i}
          data-skeleton-line=""
          aria-hidden="true"
          className="h-3 w-full animate-pulse rounded bg-zinc-700"
          style={{ width: `${100 - i * 12}%` }}
        />
      ))}
    </div>
  );
}
```

Add to `src/index.ts`:

```ts
export { EmptyState, type EmptyStateProps } from "./components/ui/empty-state";
export { Skeleton, type SkeletonProps } from "./components/ui/skeleton";
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- states`
Expected: PASS, 3 tests. Then `cd packages/nexus-design && bun test tests/` — the no-hex and barrel guards must still pass.

- [ ] **Step 5: Commit**

```bash
git add packages/nexus-design/src apps/Nexus-Dashboard/frontend/src/shell/__tests__/states.test.tsx
git commit -m "feat(design): EmptyState and Skeleton, so loading is never read as empty"
```

---

## Task 8: Density preference

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/shell/useDensity.ts`
- Create: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/useDensity.test.tsx`

**Interfaces:**
- Produces:
  - `type Density = "balanced" | "compact"`
  - `readDensity(): Density` — from `localStorage["nexus.density"]`, defaulting to `"balanced"` for missing or unrecognised values.
  - `applyDensity(d: Density): void` — sets `data-nexus-density` on `document.documentElement`.
  - `useDensity(): [Density, (d: Density) => void]`

- [ ] **Step 1: Write the failing test**

Create `apps/Nexus-Dashboard/frontend/src/shell/__tests__/useDensity.test.tsx`:

```tsx
import { describe, it, expect, beforeEach } from "vitest";
import { readDensity, applyDensity } from "../useDensity";

beforeEach(() => {
  localStorage.clear();
  document.documentElement.removeAttribute("data-nexus-density");
});

describe("readDensity", () => {
  it("defaults to balanced when nothing is stored", () => {
    expect(readDensity()).toBe("balanced");
  });

  it("reads a stored value", () => {
    localStorage.setItem("nexus.density", "compact");
    expect(readDensity()).toBe("compact");
  });

  it("falls back to balanced for a value it does not recognise", () => {
    // Otherwise a stale or hand-edited key renders the shell with no density
    // tokens at all.
    localStorage.setItem("nexus.density", "enormous");
    expect(readDensity()).toBe("balanced");
  });
});

describe("applyDensity", () => {
  it("stamps the attribute the tokens are scoped to", () => {
    applyDensity("compact");
    expect(document.documentElement.getAttribute("data-nexus-density")).toBe("compact");
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- useDensity`
Expected: FAIL — cannot resolve `../useDensity`.

- [ ] **Step 3: Implement it**

Create `apps/Nexus-Dashboard/frontend/src/shell/useDensity.ts`:

```ts
import { useCallback, useEffect, useState } from "react";

export type Density = "balanced" | "compact";

const KEY = "nexus.density";
const VALID: Density[] = ["balanced", "compact"];

/**
 * Shell-local only. This preference has the same cross-origin problem as the
 * theme — localStorage does not reach chat.tnhc.dev — and both are solved once,
 * in the theming spec, rather than twice and badly.
 */
export function readDensity(): Density {
  const raw = typeof localStorage === "undefined" ? null : localStorage.getItem(KEY);
  return VALID.includes(raw as Density) ? (raw as Density) : "balanced";
}

export function applyDensity(d: Density): void {
  document.documentElement.setAttribute("data-nexus-density", d);
}

export function useDensity(): [Density, (d: Density) => void] {
  const [density, setState] = useState<Density>(readDensity);

  useEffect(() => { applyDensity(density); }, [density]);

  const set = useCallback((d: Density) => {
    localStorage.setItem(KEY, d);
    setState(d);
  }, []);

  return [density, set];
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- useDensity`
Expected: PASS, 4 tests.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src/shell/useDensity.ts apps/Nexus-Dashboard/frontend/src/shell/__tests__/useDensity.test.tsx
git commit -m "feat(shell): shell-local density preference"
```

---

## Task 9: Apps drawer — the command palette

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/shell/AppsDrawer.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/shell/filterApps.ts`
- Create: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/AppsDrawer.test.tsx`

**Interfaces:**
- Consumes: `Overlay` (Task 6), `Pill` and `Kbd` (Task 5), `AppEntry` from `../api` — `{ id: string; name: string; description: string; url: string; path: string; health: "healthy" | "offline" }`.
- Produces:
  - `filterApps(apps: AppEntry[], query: string): AppEntry[]` — case-insensitive match on `name` or `id`; healthy entries sort before offline; stable by name within each group.
  - `AppsDrawer({ apps, open, onClose }: { apps: AppEntry[]; open: boolean; onClose: () => void })`

- [ ] **Step 1: Write the failing test**

Create `apps/Nexus-Dashboard/frontend/src/shell/__tests__/AppsDrawer.test.tsx`:

```tsx
import { describe, it, expect, vi } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import { filterApps } from "../filterApps";
import AppsDrawer from "../AppsDrawer";
import type { AppEntry } from "../../api";

function app(id: string, name: string, health: AppEntry["health"]): AppEntry {
  return { id, name, description: "", url: `https://${id}.tnhc.dev`, path: `/${id}`, health };
}

const APPS: AppEntry[] = [
  app("nexus-draw", "Draw", "healthy"),
  app("nexus-chat", "Chat", "healthy"),
  app("nexus-forge", "Forge", "offline"),
  app("nexus-vault", "Vault", "offline"),
];

describe("filterApps", () => {
  it("matches on name and on id", () => {
    expect(filterApps(APPS, "draw").map((a) => a.id)).toEqual(["nexus-draw"]);
    expect(filterApps(APPS, "nexus-vault").map((a) => a.id)).toEqual(["nexus-vault"]);
  });

  it("is case-insensitive", () => {
    expect(filterApps(APPS, "CHAT").map((a) => a.id)).toEqual(["nexus-chat"]);
  });

  it("sorts healthy before offline", () => {
    expect(filterApps(APPS, "").map((a) => a.health)).toEqual(["healthy", "healthy", "offline", "offline"]);
  });

  it("returns everything for an empty query", () => {
    expect(filterApps(APPS, "  ")).toHaveLength(4);
  });

  it("returns nothing rather than everything for no match", () => {
    expect(filterApps(APPS, "zzzz")).toEqual([]);
  });
});

describe("AppsDrawer", () => {
  function open() {
    return render(
      <MemoryRouter>
        <AppsDrawer apps={APPS} open onClose={() => {}} />
      </MemoryRouter>,
    );
  }

  it("lists offline apps, so half the ecosystem is not silently denied", () => {
    open();
    expect(screen.getByText("Forge")).toBeTruthy();
  });

  it("does not make an offline app activatable", () => {
    open();
    const offline = screen.getByText("Forge").closest("[data-app-entry]")!;
    expect(offline.getAttribute("aria-disabled")).toBe("true");
    expect(offline.tagName).not.toBe("A");
  });

  it("makes a healthy app a link to its in-shell path", () => {
    open();
    const healthy = screen.getByText("Draw").closest("a")!;
    expect(healthy.getAttribute("href")).toBe("/nexus-draw");
  });

  it("filters as you type", () => {
    open();
    fireEvent.change(screen.getByRole("searchbox"), { target: { value: "chat" } });
    expect(screen.queryByText("Draw")).toBeNull();
    expect(screen.getByText("Chat")).toBeTruthy();
  });

  it("tells you when nothing matched instead of showing an empty box", () => {
    open();
    fireEvent.change(screen.getByRole("searchbox"), { target: { value: "zzzz" } });
    expect(screen.getByText(/no apps match/i)).toBeTruthy();
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- AppsDrawer`
Expected: FAIL — cannot resolve `../filterApps`.

- [ ] **Step 3: Implement the filter**

Create `apps/Nexus-Dashboard/frontend/src/shell/filterApps.ts`:

```ts
import type { AppEntry } from "../api";

/**
 * Healthy first, then offline, alphabetical within each group.
 *
 * Offline entries are returned rather than dropped: the drawer shows them
 * dimmed and unactivatable, because hiding them would mean the shell silently
 * denies that half the ecosystem exists.
 */
export function filterApps(apps: AppEntry[], query: string): AppEntry[] {
  const q = query.trim().toLowerCase();
  const matched = q
    ? apps.filter((a) => a.name.toLowerCase().includes(q) || a.id.toLowerCase().includes(q))
    : [...apps];

  return matched.sort((a, b) => {
    if (a.health !== b.health) return a.health === "healthy" ? -1 : 1;
    return a.name.localeCompare(b.name);
  });
}
```

- [ ] **Step 4: Implement the drawer**

Create `apps/Nexus-Dashboard/frontend/src/shell/AppsDrawer.tsx`:

```tsx
import { useMemo, useState } from "react";
import { Link } from "react-router-dom";
import { Overlay } from "../../../../../packages/nexus-design/src/components/ui/overlay";
import { Pill } from "../../../../../packages/nexus-design/src/components/ui/pill";
import { EmptyState } from "../../../../../packages/nexus-design/src/components/ui/empty-state";
import { filterApps } from "./filterApps";
import type { AppEntry } from "../api";

export default function AppsDrawer({
  apps, open, onClose,
}: { apps: AppEntry[]; open: boolean; onClose: () => void }) {
  const [query, setQuery] = useState("");
  const results = useMemo(() => filterApps(apps, query), [apps, query]);

  return (
    <Overlay open={open} onClose={onClose} label="Apps">
      <div className="border-b border-zinc-600 p-3">
        <input
          type="search"
          role="searchbox"
          autoFocus
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="Search apps…"
          className="w-full bg-transparent text-sm text-zinc-100 outline-none placeholder:text-zinc-500"
        />
      </div>

      <ul className="max-h-96 overflow-y-auto p-2">
        {results.length === 0 ? (
          <li>
            <EmptyState title="No apps match that" hint="Try part of the name, or the app id." />
          </li>
        ) : (
          results.map((a) => (
            <li key={a.id}>
              {a.health === "healthy" ? (
                <Link
                  to={a.path}
                  onClick={onClose}
                  data-app-entry=""
                  className="flex items-center justify-between rounded px-3 py-2 text-sm text-zinc-200 hover:bg-zinc-700"
                >
                  <span>{a.name}</span>
                  <Pill tone="success">online</Pill>
                </Link>
              ) : (
                <div
                  data-app-entry=""
                  aria-disabled="true"
                  title="This app is not running"
                  className="flex cursor-default items-center justify-between rounded px-3 py-2 text-sm text-zinc-500"
                >
                  <span>{a.name}</span>
                  <Pill tone="neutral">offline</Pill>
                </div>
              )}
            </li>
          ))
        )}
      </ul>
    </Overlay>
  );
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- AppsDrawer`
Expected: PASS, 10 tests.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src/shell/AppsDrawer.tsx apps/Nexus-Dashboard/frontend/src/shell/filterApps.ts apps/Nexus-Dashboard/frontend/src/shell/__tests__/AppsDrawer.test.tsx
git commit -m "feat(shell): command-palette apps drawer that admits offline apps exist"
```

---

## Task 10: Health strip

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/shell/HealthStrip.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/HealthStrip.test.tsx`

**Interfaces:**
- Consumes: `Pill` (Task 5), `AppEntry` (Task 9's shape).
- Produces: `HealthStrip({ apps }: { apps: AppEntry[] })` — renders at most 8 pills, offline first so problems are visible without scanning, plus a summary count.

- [ ] **Step 1: Write the failing test**

```tsx
import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import HealthStrip from "../HealthStrip";
import type { AppEntry } from "../../api";

function app(id: string, health: AppEntry["health"]): AppEntry {
  return { id, name: id, description: "", url: "", path: `/${id}`, health };
}

describe("HealthStrip", () => {
  it("summarises how many are healthy", () => {
    render(<HealthStrip apps={[app("a", "healthy"), app("b", "offline")]} />);
    expect(screen.getByText("1 of 2 healthy")).toBeTruthy();
  });

  it("shows offline services first, because those are the ones that matter", () => {
    render(<HealthStrip apps={[app("aaa", "healthy"), app("zzz", "offline")]} />);
    const pills = screen.getAllByTestId("health-pill");
    expect(pills[0]!.textContent).toContain("zzz");
  });

  it("caps the strip rather than wrapping 86 pills across the page", () => {
    const many = Array.from({ length: 20 }, (_, i) => app(`svc-${i}`, "healthy"));
    render(<HealthStrip apps={many} />);
    expect(screen.getAllByTestId("health-pill").length).toBeLessThanOrEqual(8);
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- HealthStrip`
Expected: FAIL — cannot resolve `../HealthStrip`.

- [ ] **Step 3: Implement it**

```tsx
import { Link } from "react-router-dom";
import { Pill } from "../../../../../packages/nexus-design/src/components/ui/pill";
import type { AppEntry } from "../api";

const MAX = 8;

/** Offline first: a strip you have to read left-to-right to find the problem is
 *  a strip nobody reads. */
export default function HealthStrip({ apps }: { apps: AppEntry[] }) {
  const healthy = apps.filter((a) => a.health === "healthy").length;
  const ordered = [...apps].sort((a, b) =>
    a.health === b.health ? a.name.localeCompare(b.name) : a.health === "offline" ? -1 : 1,
  );

  return (
    <div className="flex flex-wrap items-center gap-2">
      {ordered.slice(0, MAX).map((a) => (
        <Pill key={a.id} data-testid="health-pill" tone={a.health === "healthy" ? "success" : "danger"}>
          {a.name}
        </Pill>
      ))}
      <Link to="/admin" className="text-xs text-zinc-500 hover:text-zinc-300">
        {healthy} of {apps.length} healthy
      </Link>
    </div>
  );
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- HealthStrip`
Expected: PASS, 3 tests.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src/shell/HealthStrip.tsx apps/Nexus-Dashboard/frontend/src/shell/__tests__/HealthStrip.test.tsx
git commit -m "feat(shell): health strip with offline services first"
```

---

## Task 11: WidgetShell and the four home widgets

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/pages/widgets/WidgetShell.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/pages/widgets/{Today,Unread,Activity,Pinned}.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/pages/widgets/__tests__/WidgetShell.test.tsx`

**Interfaces:**
- Consumes: `Card`, `EmptyState`, `Skeleton` (Tasks 4 and 7).
- Produces: `WidgetShell<T>({ title, state, empty, children })` where
  `state: { status: "loading" } | { status: "error"; message: string } | { status: "ready"; data: T }`.

**The rule this enforces:** an API failure must never render as an empty widget. Empty means "nothing today"; error means "we could not ask".

- [ ] **Step 1: Write the failing test**

```tsx
import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import WidgetShell from "../WidgetShell";

describe("WidgetShell", () => {
  it("shows a busy indicator while loading", () => {
    render(<WidgetShell title="Today" state={{ status: "loading" }} empty="Nothing today">{() => null}</WidgetShell>);
    expect(screen.getByRole("status").getAttribute("aria-busy")).toBe("true");
  });

  it("distinguishes an error from an empty result", () => {
    render(
      <WidgetShell title="Today" state={{ status: "error", message: "Calendar unavailable" }} empty="Nothing today">
        {() => null}
      </WidgetShell>,
    );
    expect(screen.getByText("Calendar unavailable")).toBeTruthy();
    expect(screen.queryByText("Nothing today")).toBeNull();
  });

  it("shows the empty copy only when the request actually succeeded with nothing", () => {
    render(
      <WidgetShell title="Today" state={{ status: "ready", data: [] as number[] }} empty="Nothing today">
        {(rows) => (rows.length ? <p>{rows.length}</p> : null)}
      </WidgetShell>,
    );
    expect(screen.getByText("Nothing today")).toBeTruthy();
  });

  it("renders content when there is some", () => {
    render(
      <WidgetShell title="Today" state={{ status: "ready", data: [1, 2] }} empty="Nothing today">
        {(rows) => <p>{rows.length} events</p>}
      </WidgetShell>,
    );
    expect(screen.getByText("2 events")).toBeTruthy();
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- WidgetShell`
Expected: FAIL — cannot resolve `../WidgetShell`.

- [ ] **Step 3: Implement WidgetShell**

```tsx
import type { ReactNode } from "react";
import { EmptyState } from "../../../../../../packages/nexus-design/src/components/ui/empty-state";
import { Skeleton } from "../../../../../../packages/nexus-design/src/components/ui/skeleton";

export type WidgetState<T> =
  | { status: "loading" }
  | { status: "error"; message: string }
  | { status: "ready"; data: T };

/**
 * The four states, in one place, so no widget improvises its own.
 *
 * The distinction that matters: an empty array renders the empty copy, a failed
 * request renders the error. Collapsing those two is how a broken API becomes
 * an apparently quiet day.
 */
export default function WidgetShell<T>({
  title, state, empty, children,
}: {
  title: string;
  state: WidgetState<T>;
  empty: string;
  children: (data: T) => ReactNode;
}) {
  return (
    <section className="rounded-lg border border-zinc-600 bg-zinc-800 p-4">
      <h2 className="mb-3 font-mono text-xs uppercase tracking-[0.18em] text-zinc-500">{title}</h2>
      {state.status === "loading" ? <Skeleton lines={3} /> : null}
      {state.status === "error" ? (
        <p role="alert" className="py-4 text-sm text-state-danger">{state.message}</p>
      ) : null}
      {state.status === "ready"
        ? (children(state.data) ?? <EmptyState title={empty} />)
        : null}
    </section>
  );
}
```

- [ ] **Step 4: Implement the four widgets**

Each fetches on mount, maps failure to `{ status: "error" }`, and renders through `WidgetShell`. `Today.tsx`:

```tsx
import { useEffect, useState } from "react";
import WidgetShell, { type WidgetState } from "./WidgetShell";

type Event = { id: string; title: string; startTime: string };

export default function Today() {
  const [state, setState] = useState<WidgetState<Event[]>>({ status: "loading" });

  useEffect(() => {
    let cancelled = false;
    const today = new Date().toISOString().slice(0, 10);
    fetch(`/ipa/calendar/events?from=${today}&to=${today}`)
      .then((r) => (r.ok ? r.json() : Promise.reject(new Error(String(r.status)))))
      .then((body: { events: Event[] }) => {
        if (!cancelled) setState({ status: "ready", data: body.events });
      })
      .catch(() => {
        if (!cancelled) setState({ status: "error", message: "Calendar unavailable" });
      });
    return () => { cancelled = true; };
  }, []);

  return (
    <WidgetShell title="Today" state={state} empty="Nothing scheduled today">
      {(events) =>
        events.length === 0 ? null : (
          <ul className="flex flex-col gap-1">
            {events.map((e) => (
              <li key={e.id} className="flex gap-3 border-b border-zinc-700 py-1 text-sm last:border-0">
                <span className="font-mono text-xs text-accent">{e.startTime.slice(11, 16)}</span>
                <span className="text-zinc-200">{e.title}</span>
              </li>
            ))}
          </ul>
        )
      }
    </WidgetShell>
  );
}
```

Build `Unread.tsx` against `/ipa/mail/messages?folder=inbox&unread=1` with empty copy "No unread mail" and error copy "Mail unavailable"; `Activity.tsx` against `/ipa/notifications` with "Nothing recent" / "Notifications unavailable"; `Pinned.tsx` from `listApps()` filtered to `health === "healthy"`, empty copy "No apps pinned yet". Each uses the identical `useEffect` + `cancelled` shape shown above.

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- WidgetShell`
Expected: PASS, 4 tests.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src/pages/widgets
git commit -m "feat(shell): home widgets with loading, empty, error and content kept distinct"
```

---

## Task 12: Wire the shell together

**Files:**
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/Shell.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Home.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/shell.integration.test.tsx`

**Interfaces:**
- Consumes: `AppsDrawer` (Task 9), `HealthStrip` (Task 10), the four widgets (Task 11), `useDensity` (Task 8), `Avatar` and `Kbd` (Task 5).
- Produces: no new exports. `Home` renders widgets; `<Grid/>` is reached only from the drawer's browse view.

- [ ] **Step 1: Write the failing test**

```tsx
import { describe, it, expect } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import Shell from "../Shell";

const user = { id: "usr-1", username: "eric", email: "e@tnhc.dev", role: "founder" };

describe("Shell", () => {
  function mount() {
    return render(
      <MemoryRouter>
        <Shell user={user} apps={[]}><p>content</p></Shell>
      </MemoryRouter>,
    );
  }

  it("stamps the density attribute so the density tokens apply", () => {
    mount();
    expect(document.documentElement.getAttribute("data-nexus-density")).toBe("balanced");
  });

  it("opens the apps drawer from the Apps button", () => {
    mount();
    expect(screen.queryByRole("dialog")).toBeNull();
    fireEvent.click(screen.getByRole("button", { name: /apps/i }));
    expect(screen.getByRole("dialog")).toBeTruthy();
  });

  it("opens the apps drawer with the keyboard shortcut", () => {
    mount();
    fireEvent.keyDown(document, { key: "k", metaKey: true });
    expect(screen.getByRole("dialog")).toBeTruthy();
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd apps/Nexus-Dashboard/frontend && bun run test -- shell.integration`
Expected: FAIL — `Shell` does not accept an `apps` prop.

- [ ] **Step 3: Rebuild Shell**

`Shell` currently has the signature `{ sidebar: ReactNode; user?: {...} | null; ... }`
— `sidebar` is required and receives `<Launcher/>` from the router. **Remove the
`sidebar` prop entirely** and delete its `<Launcher/>` call sites: the drawer
replaces the sidebar app list, and leaving both would reintroduce exactly the
duplication this spec exists to remove.

Modify `Shell.tsx` to accept `apps: AppEntry[]`, call `useDensity()` at the top, register a `keydown` listener for `(e.metaKey || e.ctrlKey) && e.key === "k"` that opens the drawer and calls `e.preventDefault()`, render the top bar (brand, search trigger showing `<Kbd>⌘K</Kbd>`, notification bell, `<Avatar label={user.username} />`), the left rail with the `Apps` button at the top, and `{children}` in main. Remove the `Launcher` app list from the rail — the drawer replaces it.

- [ ] **Step 4: Rebuild Home**

In `Home.tsx`, replace `<Grid />` in the signed-in branch with the health strip and the widget grid:

```tsx
<div className="flex flex-col gap-4 p-4">
  <HealthStrip apps={apps} />
  <div className="grid grid-cols-1 gap-4 md:grid-cols-2 xl:grid-cols-3">
    <div className="xl:row-span-2"><Today /></div>
    <Unread />
    <Activity />
    <div className="md:col-span-2"><Pinned /></div>
  </div>
</div>
```

- [ ] **Step 5: Run the whole frontend suite**

Run: `cd apps/Nexus-Dashboard/frontend && bun run check && bun run test`
Expected: typecheck clean; all suites pass, including the pre-existing 154.

- [ ] **Step 6: Verify responsiveness by hand**

Run `bun run dev`, then at 320px, 768px and 1280px confirm: no horizontal scrollbar on `document.body`, widgets stack to one column below 640px, and the drawer is usable at every width.

- [ ] **Step 7: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src
git commit -m "feat(shell): dashboard home is a dashboard, apps live in the drawer"
```

---

## Self-Review Notes

**Spec coverage.** Token family → Task 1. Contrast gate → Tasks 2–3. Scoped CSS with defaults on bare `:root` → Task 3. Component kit (Pill, Kbd, Avatar, Overlay, EmptyState, Skeleton) → Tasks 5–7. Density scales and preference → Tasks 1 and 8. Apps drawer with offline visible-but-dead → Task 9. Health strip → Task 10. Four widget states → Task 11. Shell layout and responsive → Task 12.

**Two spec requirements got their own tasks after inspecting the code:** the existing components hard-code six hex literals, and `@nexus/design` declares no dependencies despite importing three — both are Task 4, because the no-hex guard the spec asks for fails on day one otherwise.

**Deferred deliberately, matching the spec's Out section:** the browse view behind the palette (`See all N apps`) still routes to the existing `Grid.tsx`; a grouped browse grid is a follow-up. `Table`, `Tabs` and `Toast` are not built. Theme switching, cross-origin propagation and per-app kit adoption are the later spec.
