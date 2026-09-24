# Nexus Design Foundation and Shell

**Date:** 2026-08-26
**Status:** Approved for implementation planning

## Goal

Give the ecosystem one design language, and rebuild the dashboard as a dashboard
rather than a launcher.

Two deliverables, coupled on purpose: a token layer shaped as a **theme family**,
and a shell that renders in it. They are specified together because a ground
colour and a density scale cannot be judged as swatches — only as a working
interface.

## Scope

**In:** the token layer (four themes, two densities, generated CSS, contrast
validation); a shared component kit sufficient for the shell; the dashboard
shell — top bar, rail, apps drawer, home widgets, health strip, responsive
behaviour.

**Out:** the theme *switcher* and everything it implies — persistence,
cross-origin propagation, flash-of-wrong-theme, `prefers-contrast` interaction.
That is its own spec. Also out: shell-native chrome for apps (`proxied-app`
delivery), per-app component adoption beyond the shell, changes to tnhc.dev, and
the 82 placeholder apps.

The density preference is **shell-local** in this increment. It has the same
cross-origin problem as themes and is solved once, in the same later spec,
rather than twice and badly.

## Decisions Taken

| Decision | Choice |
|---|---|
| Ground | Void `#030303` is the default. Four themes exist as data. |
| Dashboard's job | A personal home that knows you run it: agenda, unread, activity, pinned — plus a system-health strip. |
| Density | Balanced by default; a compact scale defined and toggleable in the shell. |
| Apps | A command palette first, with a browse view behind it. |
| Reach | Tokens **and** a shared component kit. Not shell-native chrome yet. |

## Why the shell needed rebuilding

Today the home route renders the app grid, and the sidebar lists the same apps
again. The code comment acknowledges the duplication and argues for it. That
argument fails on the numbers: Cloud's registry returns **86 tools, 50 of them
offline**. A grid of 86 tiles, more than half dead, is not a directory — it is
the reason home feels like a list of links instead of a place to work.

Moving apps behind a drawer frees the whole surface for things that change hour
to hour, and turns "which apps exist" from a wall into a search.

## Token Architecture

### Shape

`packages/nexus-design/tokens/` becomes a family rather than a palette:

```
tokens/
  nexus.tokens.json        # structure, type, spacing, motion, radius, z — theme-independent
  themes/void.json         # default
  themes/abyss.json
  themes/petrol.json
  themes/slate.json
  density/balanced.json    # default
  density/compact.json
```

`src/generate.ts` emits one CSS file declaring the theme-independent tokens on
`:root`, each theme's colour tokens under `[data-nexus-theme="<id>"]`, and each
density's spacing/type tokens under `[data-nexus-density="<id>"]`. Void and
Balanced are **also** emitted on bare `:root`, so a document with no attributes
renders the default rather than nothing.

Consumers keep using the existing Tailwind alias in each app's `index.css`,
which maps the zinc scale onto `--nexus-color-*`. No component changes when a
theme or density changes; that is the whole point of the indirection.

### Themes

Every theme declares the same token names. The table below is a **representative
subset** — the full set also covers `bg-raised`, `border-strong`, `text-muted`,
`text-inverse`, `accent-hover`, `accent-active`, and the remaining state
colours. Values are a **starting point**, subject to the contrast gate in the
next section; several will move.

| Token | Void | Abyss Teal | Petrol | Slate Blue |
|---|---|---|---|---|
| `bg-canvas` | `#030303` | `#04100E` | `#07131A` | `#080D16` |
| `bg-surface` | `#0A0A0A` | `#08201C` | `#0C1F28` | `#0D1421` |
| `bg-elevated` | `#111111` | `#0C2B26` | `#112B36` | `#141D2E` |
| `border-subtle` | `rgba(255,255,255,.10)` | `rgba(140,255,225,.12)` | `rgba(150,220,255,.12)` | `rgba(160,190,255,.12)` |
| `text-primary` | `#FFFFFF` | `#F0FBF8` | `#F2F8FA` | `#F1F4FA` |
| `text-secondary` | `#A0A0A0` | `#8FA9A3` | `#93A4AC` | `#98A3B8` |
| `accent-primary` | `#CCFF00` | `#D4FF3D` | `#CCFF00` | `#C2F000` |
| `state-success` | `#2AC57D` | `#4ADE80` | `#34D399` | `#34D399` |

The accent and success values differ **by design**, and this is the substantive
colour work in this spec rather than an inconsistency:

- `#CCFF00` is a yellow-green. Against Void and Slate it has separation and
  reads as signal. Against **Abyss Teal it sits close in hue**, harmonises, and
  quietly stops meaning *this is live* — so Abyss lightens and yellows it.
- **Green-on-teal is the real failure.** A `#2AC57D` health pill on an Abyss
  canvas is mush, so Abyss raises success to a brighter green that clears its
  ground. Petrol and Slate take a middle value.
- Slate damps the accent slightly: near-complementary acid on cool blue
  vibrates at small sizes, which is fine on a marketing CTA and unpleasant on a
  row of six pills.

`state-warning`, `state-danger` and `state-info` follow the same rule: declared
per theme, tuned only where the ground demands it.

### Density

Both scales are complete; neither is derived from the other.

| Token | Balanced | Compact |
|---|---|---|
| `space-1 … space-8` | 4, 8, 12, 16, 20, 24, 32, 48 | 2, 4, 8, 12, 16, 20, 24, 32 |
| `text-xs / sm / base` | 11 / 13 / 14 | 10 / 12 / 13 |
| `text-lg / xl / 2xl` | 16 / 20 / 28 | 15 / 18 / 24 |
| `control-height` | 32 | 26 |
| `widget-padding` | `space-4` (16) | `space-3` (12) |

Balanced targets the register an instrument occupies — enough air to feel
considered, tight enough that a widget shows real content rather than three
items. It is deliberately a long way from the landing page's `p-24`, which is
right for a marketing page and wrong here.

### Contrast validation

`packages/nexus-design` gains a validator that computes WCAG contrast for every
(text, surface) and (state, surface) pair across **4 themes × 2 densities** and
**fails the build** below AA: 4.5:1 for body text, 3:1 for large text and
non-text UI. Its report is written to `dist/contrast-report.md` and committed.

This exists because only Void will be exercised on real screens in this
increment. Without it, three themes ship as untried guesses. With it, they ship
as verified data, and the later theming spec inherits something trustworthy.

## Component Kit

`packages/nexus-design/src/components/ui/` has `button`, `card`, `input`. This
increment adds only what the shell actually needs:

| Component | Why |
|---|---|
| `Pill` | Health strip, unread counts, offline tags |
| `Kbd` | The `⌘K` affordance, and every shortcut hint after it |
| `Avatar` | Identity chip in the top bar |
| `Overlay` | The apps drawer; the first modal surface in the system |
| `EmptyState` | Every widget needs one, and they must not be improvised per widget |
| `Skeleton` | Loading, distinct from empty |

`Table`, `Tabs` and `Toast` are **not** built here. The shell does not need
them, and a primitive designed without a real consumer is designed wrong. They
land with the first app that needs them.

Every component takes its values from tokens only. A hard-coded colour or pixel
value in this directory is a defect, and a test asserts the directory contains
no hex literals.

## Shell

### Layout

- **Top bar** — crystal glass (`backdrop-blur-xl` over a semi-transparent
  canvas, 1px inner stroke), carrying: brand mark, global search / `⌘K`
  trigger, notification bell with count, identity chip.
- **Left rail** — the `Apps` button at the top, primary navigation beneath it,
  the Operator link at the foot for admin roles.
- **Main** — health strip, then the widget grid.

The grid uses asymmetric spans — the landing page's bento protocol at dashboard
density. Uniform boxes are what home does today and part of why it reads as a
list.

### Apps drawer

Trigger: the `Apps` button or `⌘K` anywhere in the shell.

- **Empty state** — pinned apps, then recent, then healthy apps. Never all 86.
- **Typing** — filters all 86 by name and id. **Offline results appear, dimmed
  and unactivatable, with an `offline` tag.** Hiding them would mean the drawer
  silently denies that half the ecosystem exists; showing them as dead links
  would invite clicks that go nowhere. The existing launcher already takes this
  position for the same reason, and it is right.
- **Browse** — `See all 86 apps` switches the same overlay to a grouped grid,
  for when you are exploring rather than navigating.
- **Keyboard** — arrows move, `Enter` opens, `Escape` closes, focus returns to
  the trigger. The palette is focus-trapped while open.

### Home widgets

| Widget | Source | Notes |
|---|---|---|
| Health strip | `/ipa/apps` + service health | Pills, green at a glance, click through to `/admin` |
| Today | `/ipa/calendar` | Owner-scoped events; the overlap query means multi-day events appear |
| Unread | `/ipa/mail` | Count plus flagged |
| Activity | `/ipa/notifications` | Most recent ecosystem events |
| Pinned | `/ipa/apps` | User-chosen; falls back to healthy apps |

Pinned apps, recently-used apps and the density preference all persist in
**shell-local storage on `app.tnhc.dev`**, not server-side and not shared across
origins. This keeps them on the same footing as density: a preference the shell
owns today, and a candidate for the later cross-origin spec if it ever needs to
reach the apps themselves. Nothing here writes user preferences to a backend.

Each widget has four distinct states: **loading, empty, error, content**. An API
failure must never render as an empty widget — the Calendar spec took this
position and it holds here. Empty means "nothing today"; error means "we could
not ask".

### Responsive

| Breakpoint | Behaviour |
|---|---|
| `< 640px` | Widgets stack to one column; rail becomes a bottom bar; drawer is full-screen |
| `640–1024px` | Two-column widget grid; rail collapses to icons with labels on hover/focus |
| `> 1024px` | Full asymmetric bento; rail expanded |

No horizontal page scroll at any width. Wide content — the browse grid, any
future table — scrolls inside its own container.

## Testing

- **Tokens** — generation and flattening are already covered; extend to assert
  every theme declares every token name, and that a missing token in one theme
  fails rather than falling through to the default.
- **Contrast** — the validator has its own tests, including a known-failing
  palette that must be rejected. A validator that cannot fail is the thing this
  repository has been burned by most.
- **Components** — render, keyboard focus visible, correct roles; the no-hex
  assertion above.
- **Drawer** — filter correctness across a fixture of 86 entries; offline
  entries are present but not activatable; focus trap and restore; `Escape`.
- **Widgets** — the four states are distinguishable; an API rejection renders
  the error state and not the empty state.
- **Responsive** — no horizontal overflow at 320, 768 and 1280.

## Migration

1. Tokens and validator land first; nothing renders differently, because Void
   is the default and matches today.
2. The component kit lands with the shell as its first consumer.
3. The shell is rebuilt against it.
4. Apps inherit palette and density immediately through the existing Tailwind
   alias. Adopting the kit's components is per-app and out of scope here.

Existing user changes and the `apps/Nexus-Hosting` submodule state are
preserved throughout.

## Risks and open questions

- **Three themes will not be exercised on real screens** in this increment. The
  contrast gate is the mitigation, and it is not a complete one — it catches
  unreadable, not ugly.
- **The Tailwind zinc alias is a clever indirection**, and a second density adds
  more aliasing on top. It holds for this increment; if it grows further the
  shell should move off the zinc names onto semantic ones.
- **`design_guidelines.json` specifies Cabinet Grotesk for headings and the
  implementation dropped it** — the landing page uses Satoshi throughout, with a
  comment explaining that five weights were being fetched and referenced
  nowhere. The guidelines and the site disagree. This spec follows the
  implementation (Satoshi), and the discrepancy should be resolved deliberately
  in whichever direction rather than left as two sources of truth.
- **`apps/Nexus-Hosting/sites/nohands-company/style.css` still carries orange
  and pink gradients** from a pre-acid theme. It is a second, stale definition
  of the brand and should be reconciled or deleted.
- **86 entries in the drawer** is fine unvirtualised; if the registry grows past
  a few hundred, the browse grid needs windowing.
