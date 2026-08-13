# Apps Adopt the Design Tokens — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Draw, Chat and Cloud render in the ecosystem's palette, so the four surfaces inside the shell read as one product instead of three.

**Architecture:** Each app keeps its own variable names and its own components. Only the *values* those names resolve to change, by aliasing them to `--nexus-*` tokens. No component is rewritten. Draw is in the monorepo and imports the generated CSS directly; Chat and Cloud are separate repositories, so they get a vendored copy guarded by a drift test.

**Tech Stack:** Tailwind v4 (`@theme`, no config file), CSS custom properties, Bun, Vite.

## Decision already made — do not revisit

The founder chose **the doctrine palette (teal)** over the apps' existing violet.
`packages/nexus-design/tokens/nexus.tokens.json` is the source of truth;
Chat's and Cloud's violet is the thing that drifted. Chat's `--accent-*`
(`#7c3aed` family) and Cloud's `--purple` (`#a78bfa`) become the teal accent
`#27c9a5`. Backgrounds pick up the green-tinted near-blacks.

Same darkness throughout — a hue shift, not a light/dark flip.

## Global Constraints

- **Every app must still build standalone.** `apps/Nexus` (Chat) and
  `apps/Nexus-Cloud` are separate git repositories. They must never import
  across the submodule boundary — a standalone clone would not resolve it. They
  get a vendored copy of the generated CSS instead.
- **Generated CSS is never hand-edited.** `packages/nexus-design` owns the
  values. A vendored copy that differs from freshly generated output is a
  failing test, not a variation.
- **Status colours keep their meaning.** Red stays danger, amber stays warning,
  green stays success. Only the neutral/accent identity moves to teal.
- **No component rewrites.** If a task finds itself editing `.tsx` to change
  colours, the alias layer is wrong — fix the alias layer instead.
- Stage explicitly by path. **Never `git add -A`.**
- `apps/Nexus-Cloud/.gitignore` is a bare `*`; every tracked file there was
  force-added. New files in that repo need `git add -f`. This is the repo's
  existing convention, not a workaround.
- A pre-commit secret scanner runs. Never use `--no-verify`.
- **Never `pkill`/`killall`** — it has twice killed unrelated production
  processes here. Stop a service by the exact PID in
  `/tmp/nexus-production/pids/<name>.pid`.
- `deploy.sh bg` is idempotent and **will not pick up new code** for a running
  service. It reports success either way. Always verify the change is live.

---

### Task 1: Vendoring and the drift guard

Chat and Cloud cannot import from `packages/nexus-design`. This task gives them
a copy and makes divergence impossible to miss.

**Files:**
- Create: `packages/nexus-design/scripts/vendor.ts`
- Create: `packages/nexus-design/src/vendor.test.ts`
- Modify: `packages/nexus-design/package.json`

**Interfaces:**
- Produces: `vendorTargets(): {repo: string, dest: string}[]` — the list of
  vendored destinations, so the test and the script cannot disagree about where
  copies live.
- Produces: `npm run vendor` in `packages/nexus-design`, which regenerates and
  copies to every target.

- [ ] **Step 1: Write the failing test**

`packages/nexus-design/src/vendor.test.ts`:

```ts
import { describe, it, expect } from "vitest";
import { readFileSync, existsSync } from "node:fs";
import { join } from "node:path";
import { vendorTargets } from "../scripts/vendor";

const root = join(import.meta.dirname, "..", "..", "..");

describe("vendored token copies", () => {
  const generated = () =>
    readFileSync(join(root, "packages/nexus-design/dist/nexus-tokens.css"), "utf-8");

  it("names at least the two standalone repos", () => {
    const dests = vendorTargets().map((t) => t.repo);
    expect(dests).toContain("apps/Nexus");
    expect(dests).toContain("apps/Nexus-Cloud");
  });

  for (const target of vendorTargets()) {
    it(`${target.repo} has a vendored copy`, () => {
      expect(existsSync(join(root, target.dest))).toBe(true);
    });

    // The one that matters. A vendored file that has drifted from the
    // generator is the exact failure this whole pipeline exists to prevent,
    // and it is invisible until someone notices two apps looking different.
    it(`${target.repo}'s copy is byte-identical to the generated output`, () => {
      expect(readFileSync(join(root, target.dest), "utf-8")).toBe(generated());
    });
  }
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cd packages/nexus-design && npx vitest run src/vendor.test.ts`
Expected: FAIL — cannot find `../scripts/vendor`.

- [ ] **Step 3: Implement the script**

`packages/nexus-design/scripts/vendor.ts`:

```ts
import { copyFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";

/**
 * Where the generated token CSS is copied to.
 *
 * Chat and Cloud are separate git repositories, so they cannot import from
 * this package — a standalone clone would not resolve the path, and both must
 * keep building on their own. A vendored copy plus a drift test is the
 * trade: duplication that cannot rot silently.
 */
export function vendorTargets(): { repo: string; dest: string }[] {
  return [
    { repo: "apps/Nexus", dest: "apps/Nexus/packages/nexus-web/src/nexus-tokens.css" },
    { repo: "apps/Nexus-Cloud", dest: "apps/Nexus-Cloud/public/nexus-tokens.css" },
  ];
}

export function vendorAll(root: string): void {
  const src = join(root, "packages/nexus-design/dist/nexus-tokens.css");
  for (const t of vendorTargets()) {
    const dest = join(root, t.dest);
    mkdirSync(dirname(dest), { recursive: true });
    copyFileSync(src, dest);
  }
}
```

Add a CLI entry so the script runs standalone, and wire `"vendor"` into
`package.json` scripts so it regenerates (`build`) before copying.

- [ ] **Step 4: Run the vendor script, then the test**

```bash
cd packages/nexus-design && npm run vendor && npx vitest run
```

Expected: all tests pass, including the byte-identical assertions.

- [ ] **Step 5: Prove the drift test actually fails**

Append a stray line to one vendored copy, re-run the suite, confirm the
byte-identical test fails, then restore it with `npm run vendor`. Record this in
your report — a drift test that cannot fail is the defect it was written to
prevent.

- [ ] **Step 6: Commit**

Stage `packages/nexus-design/**` in the monorepo. The two vendored copies live
in submodules; commit each in its own repository, then bump both pointers.
Cloud's copy needs `git add -f`.

---

### Task 2: Draw adopts the tokens

Draw is Tailwind-only — roughly 90 `zinc-*` utility usages and no CSS custom
properties. Remapping the `zinc` scale converts every one of them without
touching a single component.

**Files:**
- Modify: `apps/Nexus-Draw/frontend/src/index.css`
- Modify: `apps/Nexus-Draw/frontend/package.json`

- [ ] **Step 1: Make the build regenerate tokens**

Copy the working precedent from `apps/Nexus-Dashboard/frontend/package.json`:
both `dev` and `build` `cd` into `packages/nexus-design`, run its build, and
come back. Match it exactly — Draw sits at the same depth.

- [ ] **Step 2: Import the theme and remap the greys**

`apps/Nexus-Draw/frontend/src/index.css` currently contains only
`@import "tailwindcss";`. Add the theme import, then a `@theme` block
overriding Tailwind's `zinc` scale:

```css
@import "tailwindcss";
@import "../../../../packages/nexus-design/dist/nexus-theme.css";

/* Draw styles entirely with Tailwind's zinc scale and has no variables of its
   own. Rather than rewrite ~90 utility usages across every component, point
   the scale itself at the ecosystem tokens: `bg-zinc-800` keeps working and
   starts rendering in the ecosystem's palette. The mapping runs dark-to-light
   the way Tailwind's scale does, so contrast relationships survive. */
@theme {
  --color-zinc-900: var(--nexus-color-bg-canvas);
  --color-zinc-800: var(--nexus-color-bg-surface);
  --color-zinc-700: var(--nexus-color-bg-elevated);
  --color-zinc-600: var(--nexus-color-border-strong);
  --color-zinc-500: var(--nexus-color-text-muted);
  --color-zinc-400: var(--nexus-color-text-secondary);
  --color-zinc-300: var(--nexus-color-text-secondary);
  --color-zinc-200: var(--nexus-color-text-primary);
}
```

Verify these are the Tailwind v4 theme variable names for the colour scale
before relying on them; if v4 names them differently, use the correct names and
say so in your report.

- [ ] **Step 3: Build and confirm the tokens reach the output**

```bash
cd apps/Nexus-Draw/frontend && npm run build
grep -c "nexus-color" dist/assets/*.css
```

Expected: build succeeds and the compiled CSS contains `nexus-color`
references. A zero count means Tailwind tree-shook the theme and the mapping is
not being applied — investigate rather than forcing it.

- [ ] **Step 4: Run the suite**

Run: `cd apps/Nexus-Draw/frontend && npx vitest run`
Expected: 115 passing, unchanged. This task changes no behaviour.

- [ ] **Step 5: Ship and verify live**

`draw.tnhc.dev` is a **Nexus-Hosting static site (site id 5)**, not served by
any `deploy.sh` process. Follow the REST deploy pipeline; a worked example is in
the preserved ledger at
`docs/superpowers/plans/2026-08-13-ecosystem-shell-execution-ledger.md`, and the
deploy token is at `~/.config/nexus-cli-nodejs/.deploy-token` (mode 0600 —
never print it).

Then assert the live `index.html` references the asset hashes from the build you
just made, and that the live CSS contains `nexus-color`.

- [ ] **Step 6: Commit**

Stage the two modified files by path.

---

### Task 3: Chat adopts the tokens

**Files:**
- Modify: `apps/Nexus/packages/nexus-web/src/index.css`

- [ ] **Step 1: Import the vendored copy and alias the palette**

The vendored `nexus-tokens.css` from Task 1 sits beside `index.css`. Import it,
then point Chat's existing `:root` variables at the tokens. Keep every variable
name — components reference them and must not be touched.

```css
@import "./nexus-tokens.css";

:root {
  --bg-900: var(--nexus-color-bg-canvas);
  --bg-800: var(--nexus-color-bg-canvas);
  --bg-700: var(--nexus-color-bg-surface);
  --bg-600: var(--nexus-color-bg-elevated);
  --bg-500: var(--nexus-color-bg-elevated);
  --fg:     var(--nexus-color-text-primary);
  --muted:  var(--nexus-color-text-muted);
  --accent-300: var(--nexus-color-accent-primary);
  --accent-400: var(--nexus-color-accent-primary);
  --accent-500: var(--nexus-color-accent-primary);
  --accent-600: var(--nexus-color-accent-primaryActive);
}
```

Read the file first: preserve any variables not listed here, and keep the
`--color-*` aliases that already point at these names working.

- [ ] **Step 2: Build**

Run: `cd apps/Nexus/packages/nexus-web && npm run build`
Expected: succeeds.

- [ ] **Step 3: Run the suite**

Run: `npx vitest run`
Expected: 10 passing, unchanged.

- [ ] **Step 4: Ship and verify live**

Caddy serves `apps/Nexus/packages/nexus-web/dist` from disk, so the build is the
deploy. Confirm the live `chat.tnhc.dev` asset hashes match the fresh build and
that the served CSS contains `nexus-color`.

- [ ] **Step 5: Commit**

Chat is a separate repository: commit there, then bump the pointer in the
monorepo.

---

### Task 4: Cloud adopts the tokens

**Files:**
- Modify: `apps/Nexus-Cloud/src/api/handlers.ts`
- Modify: `apps/Nexus-Cloud/public/status.html`
- Modify: `apps/Nexus-Cloud/src/api/handlers.dashboard.test.ts`

- [ ] **Step 1: Serve the vendored stylesheet**

Cloud serves `public/status.html` from a handler. Add a route
`GET /nexus-tokens.css` serving `public/nexus-tokens.css` (vendored in Task 1)
with `Content-Type: text/css`. Follow `handleDashboard`'s shape.

- [ ] **Step 2: Link it and alias the palette**

In `status.html`, add `<link rel="stylesheet" href="/nexus-tokens.css">` to the
`<head>`, before the existing `<style>`. Then point Cloud's `:root` variables at
the tokens, keeping every name:

```css
--bg:          var(--nexus-color-bg-canvas);
--bg-hover:    var(--nexus-color-bg-elevated);
--bg-active:   var(--nexus-color-bg-elevated);
--text:        var(--nexus-color-text-primary);
--text-muted:  var(--nexus-color-text-muted);
--border:      var(--nexus-color-border-subtle);
--purple:      var(--nexus-color-accent-primary);
--green:       var(--nexus-color-state-success);
--blue:        var(--nexus-color-state-info);
--amber:       var(--nexus-color-state-warning);
--red:         var(--nexus-color-state-danger);
```

Read the actual `:root` block first — map every variable it defines, and leave
`--sidebar-w`, `--topbar-h` and the radii alone unless a token matches exactly.
The `*-dim` translucent variants stay as they are.

- [ ] **Step 3: Extend the dashboard test**

Add to `handlers.dashboard.test.ts`: `GET /nexus-tokens.css` returns 200 with
`content-type: text/css` and a body containing `--nexus-color-accent-primary`.

- [ ] **Step 4: Run the suite**

Run: `cd apps/Nexus-Cloud && bun test`
Expected: 88 → 89 pass; the 2 pre-existing `nexus-certificate` failures remain
and must not increase.

- [ ] **Step 5: Ship and verify live**

Restart Cloud (stop the exact PID in `/tmp/nexus-production/pids/cloud.pid`,
then `./deploy/production/deploy.sh bg`). Then confirm:
`cloud.tnhc.dev/nexus-tokens.css` returns 200 `text/css`; `cloud.tnhc.dev/`
still returns the full HTML console; `?embed=1` still injects
`class="embedded"`; and all six hosts still serve.

- [ ] **Step 6: Commit**

Cloud is a separate repository: commit there (`git add -f` for the vendored
file), then bump the pointer.

---

## Final verification

With all four shipped, open `https://app.tnhc.dev` signed in and step through
`/a/nexus-draw`, `/a/nexus-chat` and `/a/nexus-cloud`. The four surfaces should
share one ground, one text colour and one accent. Note anything that still looks
foreign — leftover hardcoded colour is the expected residue, and finding it is
the point of doing this before 300 more apps exist.
