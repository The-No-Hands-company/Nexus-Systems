# Nexus Theme & Design Guide

How every Nexus app should look, and how to make it so. Written 2026-11-06 from the
token source and the apps as they are today; where this guide and the code disagree,
the code is right and this guide has a bug.

**Single source of truth:** `packages/nexus-design/tokens/` (`nexus.tokens.json`,
`themes/*.json`, `density/*.json`). Everything else — the generated CSS, the contrast
report, the vendored copies — is derived from it. This document explains how to use
those tokens; it does not redefine them.

---

## 1. Principles

1. **A flat, near-black canvas.** The default theme (`void`) paints `#030303`.
   No decorative gradients, radial colour washes, backdrop blur, glassmorphism, or glows.
   Depth comes from surface lightness, not shadow.
2. **One accent.** Acid lime `#CCFF00` marks the primary action, focus, selection and
   links. There is no second accent hue. Text that sits on the accent uses `text.inverse`.
3. **Colour means something.** Success, warning, danger and info colours describe
   *status*. Don't use them to decorate. Don't use the accent to mean a status.
4. **Surfaces layer by lightness:** `canvas` → `surface` → `elevated` → `raised`.
5. **Contrast is a build-time contract.** Body text needs 4.5:1; muted text, state
   colours and the accent need 3:1 as non-text UI. `bun run build` in
   `packages/nexus-design` checks all four themes (196 pairs), writes
   `contrast-report.md`, and refuses to generate any CSS if a pair fails — a palette
   that doesn't meet contrast cannot ship.
6. **Tokens, not literals.** If a token exists for a colour, radius, size or spacing,
   use it. A hard-coded hex is a theme that can't change.
7. **Mobile is a first-class surface** (see §5), not a responsive afterthought.
8. **A framed app doesn't repeat the shell.** Inside the Nexus shell the wordmark,
   the way home and the account chip already exist; the app shows only its own controls.

---

## 2. Token reference (default `void` theme)

### Colour

| Role | Token | Value |
|---|---|---|
| Page background | `--nexus-color-bg-canvas` | `#030303` |
| Cards, panels | `--nexus-color-bg-surface` | `#0A0A0A` |
| Popovers, hover surfaces | `--nexus-color-bg-elevated` | `#111111` |
| Raised controls, hairlines | `--nexus-color-bg-raised` | `#171717` |
| Primary text | `--nexus-color-text-primary` | `#FFFFFF` |
| Secondary text | `--nexus-color-text-secondary` | `#A0A0A0` |
| Muted text, placeholders | `--nexus-color-text-muted` | `#6E6E6E` |
| Text on the accent | `--nexus-color-text-inverse` | `#030303` |
| Accent (+ hover / active) | `--nexus-color-accent-primary` / `-hover` / `-active` | `#CCFF00` / `#B8E600` / `#A3CC00` |
| Success / Warning / Danger / Info | `--nexus-color-state-{success,warning,danger,info}` | `#2AC57D` / `#E8B24A` / `#E15D5D` / `#5CA8FF` |
| Borders | `--nexus-color-border-subtle` / `-strong` | `rgba(255,255,255,0.36)` / `0.44` |

`border.*` are alpha-white lines tuned to meet 3:1 against every surface; the shared
`Card` and `Input` use `border.subtle`. Some app code draws passive dividers with a
surface tone instead (the Dashboard's `border-zinc-700` resolves to `bg.elevated`).
Use the border tokens for anything interactive.

Three more themes exist — `abyss`, `petrol`, `slate` — selected with
`data-nexus-theme="…"` on `<html>`. **Nothing in the apps sets that attribute yet**, so
every app renders `void`. A theme switcher is unbuilt (see §8).

### Typography

- **Family: Figtree** in every app (the token now says so; it used to name Satoshi, which
  was never loaded). Weights 400 / 500 / 600 / 700. Load it with the Google Fonts link the
  apps use, or self-host via `@fontsource-variable/figtree` where a CSP forbids it (Hosting).
- **Mono: JetBrains Mono, then the system monospace** — the brand's face for code
  (`handbook/brand.md`, live at tnhc.dev/brand). No app loads JetBrains Mono yet, so they
  fall back to the system face; don't add a font download to an app that doesn't show code.
  Hosting currently ships Fira Code instead (see §8).
- Line height: tight 1.2 · normal 1.45 · relaxed 1.7.
- Sizes come from the density scale (below), exposed as `--nexus-typography-size-*`.

### Shape, depth, motion

| | Tokens |
|---|---|
| Radius | `sm 6` · `md 10` · `lg 14` · `pill 999` (px) |
| Shadow | `sm 0 2px 8px` · `md 0 8px 24px` · `lg 0 16px 40px` (black, 18–32 %) — for floating layers only |
| Motion | `fast 120ms` · `normal 180ms` · `slow 280ms`; easing `standard` / `emphasized` |
| Z-index | base 1 · header 50 · overlay 100 · modal 200 · toast 300 |

### Density (`data-nexus-density` on `<html>`)

| | Balanced (default) | Compact |
|---|---|---|
| Spacing 1–8 | 4 8 12 16 20 24 32 48 | 2 4 8 12 16 20 24 32 |
| Type xs / sm / base / lg / xl / 2xl | 11 13 14 16 20 28 | 10 12 13 15 18 24 |
| Control height | 32 | 26 |
| Widget padding | 16 | 12 |

Only the Dashboard consumes density today.

---

## 3. Adopting the system in an app

**Import `nexus-tokens.css`, never `nexus-theme.css`.** `nexus-theme.css` only defines
Tailwind's un-prefixed `@theme` names; if nothing references them, Tailwind tree-shakes the
block and the app silently renders in stock zinc. This already happened once.

### A. App that lives in the monorepo (Dashboard, Calendar, Project, Draw)

```css
@import "tailwindcss";
@import "../../../../packages/nexus-design/dist/nexus-tokens.css";

@theme {
  /* Alias Tailwind's scales to the tokens instead of rewriting every class. */
  --color-zinc-900: var(--nexus-color-bg-canvas);
  --color-zinc-800: var(--nexus-color-bg-surface);
  --color-zinc-700: var(--nexus-color-bg-elevated);
  --color-zinc-600: var(--nexus-color-border-strong);
  --color-zinc-500: var(--nexus-color-text-muted);
  --color-zinc-400: var(--nexus-color-text-secondary);
  --color-zinc-300: var(--nexus-color-text-secondary);
  --color-zinc-200: var(--nexus-color-text-primary);
  --color-zinc-100: var(--nexus-color-text-primary);
  --color-accent: var(--nexus-color-accent-primary);
  --color-accent-hover: var(--nexus-color-accent-hover);
  --color-accent-active: var(--nexus-color-accent-active);
  --color-accent-foreground: var(--nexus-color-text-inverse);
  --color-state-success: var(--nexus-color-state-success);
  --color-state-warning: var(--nexus-color-state-warning);
  --color-state-danger: var(--nexus-color-state-danger);
  --color-state-info: var(--nexus-color-state-info);
}
```

The Dashboard's `frontend/src/index.css` is the fullest reference (it also wires
density to Tailwind's spacing and `text-*` scales). `bun run build` in the app runs
`packages/nexus-design`'s build first, so the generated CSS exists.

### B. App in its own repository (Chat → `apps/Nexus`, Hosting)

These cannot import from the monorepo (a standalone clone or Hosting's Docker build
would not resolve the path), so they carry a **vendored copy** of the generated file:

| App | Vendored file |
|---|---|
| Chat | `apps/Nexus/packages/nexus-web/src/nexus-tokens.css` |
| Hosting | `apps/Nexus-Hosting/artifacts/federated-hosting/src/nexus-tokens.css` |

Never edit those files. To refresh after a token change:

```bash
cd packages/nexus-design && bun run vendor     # regenerates dist/, copies to every target
bun test tests/                                # vendor.test.ts: copies must be byte-identical
```

The target list is `vendorTargets()` in `packages/nexus-design/scripts/vendor.ts`. A new
standalone app is added there. **The drift test is not part of any app's `check.sh`** —
it was failing, unnoticed, until 2026-11-06 (Chat's copy was an older teal palette and
a retired target was missing). Run it whenever tokens change.

### C. shadcn-style apps (Hosting)

Keep shadcn's variable names but make each one an alias of a token, so components
re-theme without edits:

```css
:root {
  --background: var(--nexus-color-bg-canvas);
  --card:       var(--nexus-color-bg-surface);
  --primary:    var(--nexus-color-accent-primary);
  --primary-foreground: var(--nexus-color-text-inverse);
  --muted-foreground:   var(--nexus-color-text-secondary);
  /* … see apps/Nexus-Hosting/artifacts/federated-hosting/src/index.css */
}
@theme inline { --color-background: var(--background); /* … */ }
```

Raw `red/amber/green/emerald/blue` utilities are remapped to the matching state token in
the same `@theme` block, so `text-red-400` renders in the shared danger colour.

---

## 4. Shared components

- `packages/nexus-design/src/components/ui/` — `avatar`, `badge`, `button`, `card`,
  `empty-state`, `input`, `kbd`, `overlay`, `page-header`, `pill`, `skeleton`, `stat`, `table`.
- `packages/nexus-app-shell` — `AppHeader`, the router-independent header for apps served
  from their own origin. Pass `embedded` when the shell frames the app (see below).
- Per-app launcher icons: `apps/Nexus-Dashboard/frontend/src/components/appMeta.ts` maps
  an app id to a Phosphor icon and a hue, with a deterministic hash fallback. These hues
  are the one sanctioned use of non-accent colour for identity, and only on the small
  icon badge — never on text, borders or surfaces.

---

## 5. Mobile and framed-app rules

These came out of testing on a phone; each fixes something that was broken.

- **44 px minimum touch targets** on anything tappable (`min-h-11 min-w-11`).
- **`dvh`, not `vh`/`screen`,** for full-height layouts (`h-dvh`). `100vh` is taller than
  the visible area while a mobile browser's address bar is showing and clips the bottom.
- **`viewport-fit=cover`** in `index.html`, and `env(safe-area-inset-bottom)` padding on
  anything pinned to the bottom edge. Without the meta tag the inset is always 0.
- **The shell's rail is a bottom bar below `sm`.** One element, restyled — not a second
  nav — with Home, Apps, Report (and Cloud/Terminal for operators). The framed app scrolls
  inside; the bar stays put, so there is always a thumb-reachable way back to the dashboard.
- **Framed apps detect `?embed=1`** and drop their own brand/home link
  (`AppHeader embedded`, Draw's `isEmbedded()`). Standalone, the "Nexus" link must point at
  the dashboard host, not `/` (which is the app itself).
- **Treat a failed request as a failure, not as "empty" or "signed out".** The Dashboard's
  `session()` returns `signed-in | signed-out | unreachable`; only a real 401 means signed
  out, and brief network drops are retried. Show a Retry state; re-check on `visibilitychange`
  and `online`.

---

## 6. Review checklist (new app or UI change)

- [ ] Tokens imported via the right path (§3); no `nexus-theme.css`.
- [ ] No hard-coded hex/rgba where a token exists; no `hsl()` literals.
- [ ] No gradients, glass blur, glows or radial washes.
- [ ] Exactly one accent; state colours used only for status.
- [ ] Figtree is loaded and set on `body`.
- [ ] Tappable things ≥ 44 px; layout uses `dvh`; `viewport-fit=cover` present.
- [ ] Works framed (`?embed=1`) without a duplicate header, and standalone with a link home.
- [ ] Loading, empty, error and offline states are distinct.
- [ ] If tokens changed: `bun run vendor` and `bun test tests/` in `packages/nexus-design`.

---

## 7. Where each app stands (verified 2026-11-06)

| App | Tokens | Notes |
|---|---|---|
| Dashboard (+ Mail, Cloud views) | monorepo import | Reference implementation; only app using density |
| Calendar | monorepo import | |
| Project | monorepo import | |
| Draw | monorepo import | Had no Figtree and no accent mapping, so its 21 `accent` classes (incl. every focus outline) rendered nothing; fixed 2026-11-06 |
| Chat web (`apps/Nexus/packages/nexus-web`) | vendored | Was on a stale teal palette; refreshed. Has its own density setting |
| Hosting | vendored | Was on its own navy/purple palette; moved to tokens. See gaps below |
| Cloud | — | No UI (frontend retired) |
| Chat desktop (`apps/Nexus/crates/nexus-desktop`) and mobile | **not covered** | Separate codebases with their own theme presets; `apps/Nexus/DESIGN.md` describes that older teal/purple direction and has not been reconciled with this guide |

---

## 8. Known gaps and open decisions

1. **Theme switching is unbuilt.** Four themes are generated and contrast-checked, but no UI
   sets `data-nexus-theme`. Apps are separate documents on separate origins (and several are
   iframed), so a switcher needs a propagation mechanism — a shared cookie on the parent
   domain, or the shell passing the choice to framed apps — before any UI is worth building.
2. **Categorical colours have no palette.** Hosting colour-codes HTTP methods, federation
   event types and site types with purple, violet, orange and yellow, which the system
   doesn't define. They were left alone (remapping them to a state colour would merge
   distinct categories). They need a small defined categorical set.
3. **Hosting's former purple "secondary" now equals the accent.** Anything that must be
   told apart from the primary names a state colour explicitly (the dashboard chart's second
   series uses `state-info`), and the shared `Button`/`Badge` `secondary` variants are
   neutral so they stay below the lime primary. Stat tiles that used lime and purple icons
   now both use lime; each still has its own icon and label.
4. **The vendor drift test isn't in any gate.** See §3B.
5. **Hosting still bundles Fira Code,** not the brand's JetBrains Mono. Swapping it needs the
   `@fontsource-variable/jetbrains-mono` package added to Hosting and its lockfile
   regenerated, which was not attempted here.
6. **Chat desktop and mobile clients** are on the older teal/purple direction (see
   `apps/Nexus/DESIGN.md`, which now says so) and need their own migration.

---

## 9. Publishing

This file is repo-local. It is written to lift cleanly into a public brand page
(tnhc.dev/brand): §1 and §2 are the brand statement and token reference; §3–§6 are
developer-facing and would sit under a "build with Nexus" section. The brand site is a
separate project and has not been touched.
