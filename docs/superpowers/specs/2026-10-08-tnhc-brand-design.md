# TNHC Handbook, Chapter 2: Brand — Design

**Date:** 2026-10-08
**Status:** Approved in conversation, awaiting written-spec review
**Programme:** Handbook (1 Charter — done → **2 Brand** → 3 Design system → 4 Voice → 5 Engineering → 6 Rollout)

## Goal

One TNHC identity, written down and published: the logo, the colour, the typeface, light mode, and how the names are used. Every later chapter and every surface (tnhc.dev, app.tnhc.dev, zajfan.tnhc.dev, the Android app) builds on it.

## Decisions

| Topic | Decision |
|---|---|
| Colour | Acid green `#CCFF00` is the TNHC colour, on the logo and in the interface |
| Logo | The warning-sign triangle, redrawn: three layers (green rim, black ring, green field); figure with a large round head, heavy body, wide stance and **hands hidden behind the head**; no text inside the sign |
| Typeface | **Figtree** (SIL Open Font License) for everything; **JetBrains Mono** (OFL) for code |
| Brand architecture | One brand. TNHC is the only mark; "Nexus" names the products ("Nexus Chat"); zajfan.tnhc.dev is the founder's page in TNHC's look; the Zajfan Standard is a TNHC document named after its author |
| Light mode | Dark first; a defined light mode that every app follows from the system setting |
| Delivery | `brand.md` in the handbook + generated assets beside it; tokens for code are Chapter 3; changing each surface's look is Chapter 6 |

### Why not Satoshi

tnhc.dev uses Satoshi today. Its licence (ITF Free Font License) forbids publishing the font files in a public repository, modifying or converting them, and making them available to third parties — so self-hosters of our apps could not use it, and tnhc.dev's public repository is currently publishing the files. This breaks the Charter's "open source, no lock-in". Figtree is the closest open typeface.

## Design

### 1. The logo

**Master file:** `brand/logo/tnhc-sign.svg` in the handbook repo, hand-authored, viewBox `0 0 240 216`. Every other logo file is generated from it.

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 240 216">
  <path d="M120 10 L232 204 L8 204 Z" fill="#CCFF00" stroke="#CCFF00" stroke-width="16" stroke-linejoin="round"/>
  <path d="M120 26 L218.1 196 L21.9 196 Z" fill="#0A0A0A" stroke="#0A0A0A" stroke-width="10" stroke-linejoin="round"/>
  <path d="M120 44 L202.5 187 L37.5 187 Z" fill="#CCFF00" stroke="#CCFF00" stroke-width="6" stroke-linejoin="round"/>
  <g transform="translate(16.8 25.2) scale(0.86)">
    <g fill="#0A0A0A"><circle cx="120" cy="91" r="16"/><rect x="107" y="110" width="26" height="38" rx="4"/></g>
    <g fill="none" stroke="#0A0A0A" stroke-width="16" stroke-linecap="round"><path d="M114 144 L88 176"/><path d="M126 144 L152 176"/></g>
    <g fill="none" stroke="#0A0A0A" stroke-width="13" stroke-linecap="round" stroke-linejoin="round"><path d="M110 116 L87 108 L108 86"/><path d="M130 116 L153 108 L132 86"/></g>
    <circle cx="120" cy="91" r="16" fill="#0A0A0A"/>
  </g>
</svg>
```

**Generated versions:**

| File | What it is |
|---|---|
| `tnhc-sign-mono.svg` | One colour (`#0A0A0A`): the ring and the figure, field and rim transparent — for print, stamps, embossing |
| `tnhc-lockup-dark.svg` / `tnhc-lockup-light.svg` | Sign + "THE NO HANDS COMPANY" in Figtree Black, set to the sign's right, text converted to outlines (no font needed to display it); text `#EDEDED` on dark, `#0A0A0A` on light |
| `tnhc-app-icon.svg` | The sign centred on a `#030303` rounded square (corner radius 22% of the side) |
| PNG icons | App icon at 16, 32, 48, 180, 192, 512 px; `favicon.ico` (16/32/48); GitHub avatar 500 px; Android launcher 48/72/96/144/192 px and Play Store 512 px |

**Rules:**
- **Clear space** around the sign: at least the width of the head (13% of the sign's width) on every side.
- **Minimum size:** the sign 16 px wide; the lockup 120 px wide.
- **Never:** stretch or rotate it, recolour it outside the three versions above, put text or another image inside the sign, add effects (shadows, gradients, outlines), or place the full-colour sign on an acid background.

### 2. Colour

| Role | Dark (default) | Light |
|---|---|---|
| Background | `#030303` void | `#F4F4F0` off-white |
| Surface | `#0D0D0D` | `#FFFFFF` |
| Text | `#EDEDED` | `#0A0A0A` |
| Muted text | `#A8A8A8` | `#55554F` |
| Accent | `#CCFF00` — fills, and text | `#CCFF00` — fills only, with `#0A0A0A` on top; never text |
| On accent | `#030303` | `#0A0A0A` |

Every text/background pair above meets WCAG AA (4.5:1); text on background meets AAA (7:1). Acid as text on a light background (1.1:1) is forbidden. Apps follow the system's light/dark setting; dark is shown when no setting is known.

### 3. Typeface

- **Figtree** for everything, weights 400, 500, 700 and 900. Headings 700–900, body 400, the name in the lockup 900 uppercase.
- **JetBrains Mono** for code, weights 400 and 700.
- Both are served from TNHC's own servers (no font CDN), with their OFL licence files beside them.
- **tnhc.dev switches from Satoshi to Figtree in this chapter** (licence fix, not a redesign): the Satoshi files and notice are deleted from the current tree, and `scripts/vendor-fonts.sh` fetches Figtree from its official repository instead. Satoshi remains in the repository's history; rewriting a public repository's history is not part of this chapter.

### 4. Names

- **The No Hands Company** in full at first mention on a page, **TNHC** after. Never "No Hands", "NoHands" or "The No-Hands Company".
- Products: **Nexus** + plain noun — "Nexus Chat", "Nexus Cloud", "Nexus Email". Where the maker must be clear: "Nexus Chat, by The No Hands Company". Nexus has no separate logo.
- **zajfan.tnhc.dev** is the founder's page and uses TNHC's look. The **Zajfan Standard** is TNHC's coding standard, named after its author.
- The domain is written **tnhc.dev** (lower case).

### 5. Publication

- Handbook: `brand.md` (CC BY 4.0, like the rest of the handbook) and `brand/` with the master, generated files, and `brand/README.md` saying which file to use where. README lists `brand.md`.
- `scripts/build-brand.py` generates every file in §1 from the master (SVG variants by direct construction, so the output is deterministic; PNG/ICO through ImageMagick; lockup text outlined from Figtree Black with fontTools).
- `scripts/check-brand.sh` (run beside `check-charter.sh`): rebuilds the SVG variants and fails if any differs from the committed file; fails if a PNG/ICO is missing or the wrong size; computes the contrast of every colour pair in §2 and fails below the stated level; fails if `brand.md` is missing a section or the README does not list it.
- tnhc.dev: `/brand` page generated from `brand.md` by `build-markdown-page.py` (like `/charter`), the logo files copied to `frontend/public/brand/` by `webmaster-sync.sh` and offered as downloads on the page, "Brand" link in the shared footer, sitemap entry.
- GitHub organisation avatar: the 500 px PNG is provided; the founder uploads it in GitHub's settings (step-by-step instructions given at hand-off).

## Testing

- `check-brand.sh` as above, plus one test proving it fails: a hand-edited variant, a wrong-size PNG and a low-contrast colour pair are each caught.
- The rendered logo is inspected as PNG at 512, 64 and 16 px on void and on off-white before hand-off.
- tnhc.dev: build succeeds; `/brand` returns 200 with the logo images loading; no `satoshi` file or reference remains in the current tree (`grep -ri satoshi frontend/` empty); pages render in Figtree.

## Out of scope

- Tokens consumed by code (CSS variables, Kotlin theme) — Chapter 3.
- Putting the new logo, colours and font on tnhc.dev's header, the dashboard, zajfan.tnhc.dev and the Android app — Chapter 6 (only tnhc.dev's font swap happens now, because it is a licence problem).
- Voice and wording — Chapter 4.
