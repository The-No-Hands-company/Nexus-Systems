# TNHC Brand Chapter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish the TNHC brand (logo, colour, typeface, names, light mode) as handbook chapter 2 with generated logo files and a guard, live at tnhc.dev/brand; switch tnhc.dev from Satoshi to Figtree.

**Architecture:** One hand-drawn master SVG in the handbook repo; `scripts/build-brand.py` builds every other logo file from its four id'd layers (SVG deterministically, PNG/ICO through ImageMagick); `scripts/check-brand.sh` fails on drift. tnhc.dev renders `brand.md` like `/charter` and serves the logo files, copied in by `webmaster-sync.sh`.

**Tech Stack:** Python 3.14 + fontTools 4.63 + ImageMagick 7 (handbook); React/CRA via craco, Tailwind, bun test (tnhc.dev).

**Spec:** `docs/superpowers/specs/2026-10-08-tnhc-brand-design.md` (Nexus-Systems repo)

## Global Constraints

- Colours exactly: acid `#CCFF00`; dark background `#030303`, surface `#0D0D0D`, text `#EDEDED`, muted `#A8A8A8`; light background `#F4F4F0`, surface `#FFFFFF`, text `#0A0A0A`, muted `#55554F`; ink in the logo `#0A0A0A`.
- Acid green is never text on a light background.
- Typeface Figtree (weights 400, 500, 700, 900) + JetBrains Mono; both self-hosted with their OFL licence files; no font CDN at runtime.
- Figtree files come from `github.com/erikdkennedy/figtree` pinned at commit `032dfa7fe219ef3a02890d6d3add84eacc9aebfe`.
- The master `brand/logo/tnhc-sign.svg` is the only hand-drawn logo file; generated files are never hand-edited.
- Names: "The No Hands Company", "TNHC", "Nexus Chat" etc.; the domain `tnhc.dev` lower case.
- Repos: handbook = `~/tnhc-charter-work/handbook` (github.com/The-No-Hands-company/handbook); tnhc.dev = `/run/media/zajferx/Data/dev/The-No-hands-Company/tnhc.dev`. Both: branch `main`, push directly, commit messages `type: description`.
- Never commit in `/run/media/zajferx/Data/dev/The-No-hands-Company` itself (the org folder). Never print secrets. No `bun install`/`pnpm install` in the Nexus-Systems monorepo.
- Every commit ends with:
  ```
  Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_01WnNDWvqmDN7u5Y3qd3kzUF
  ```
- Out of scope: tokens for code (Chapter 3); putting the logo/colours on the site header, dashboard, zajfan.tnhc.dev or Android (Chapter 6); copy rewrites (Chapter 4). tnhc.dev's favicon and title stay as they are.

---

### Task 1: The Brand chapter in the handbook — logo master, generated files, text, guard

**Files:**
- Create: `brand/logo/tnhc-sign.svg`, `brand/fonts/Figtree-Black.ttf`, `brand/fonts/OFL.txt`, `brand/README.md`, `brand.md`
- Create: `scripts/build-brand.py`, `scripts/check-brand.sh`, `scripts/tests/check-brand.test.sh`
- Generate (commit): `brand/logo/tnhc-sign-mono.svg`, `brand/logo/tnhc-lockup-dark.svg`, `brand/logo/tnhc-lockup-light.svg`, `brand/logo/tnhc-app-icon.svg`, `brand/icons/**`
- Modify: `README.md` (chapter list)

**Interfaces:**
- Produces: `scripts/build-brand.py` with module-level `PNGS: dict[str,int]` (path under `brand/` → square size) and `ICO = "icons/favicon.ico"`; CLI `--svg-only --out DIR`. Generated file names exactly as listed above. Task 3 copies `brand/logo/*.svg`, `brand/icons/app-icon-512.png`, `brand/icons/favicon.ico`.
- Produces: `scripts/check-brand.sh` — prints `PASS` or `FAIL: …` lines; env `ROOT` (repo root) and `BRAND_EXTRA_PAIR="fg bg min"` (test hook).
- Produces: `brand.md` with first line `# The No Hands Company Brand` and sections exactly `## Logo`, `## Colour`, `## Typeface`, `## Names`, `## Files` (Task 3 renders it).

- [ ] **Step 1: Write the guard and its test first**

Write `scripts/check-brand.sh` exactly:

```bash
#!/usr/bin/env bash
# Fails when the published brand drifts from its source:
#  - a generated logo SVG differs from what the master produces,
#  - an icon is missing or the wrong size,
#  - a colour pair in brand.md falls below its contrast level,
#  - brand.md lacks a section, or the README does not list it.
set -euo pipefail
ROOT="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$ROOT"
fail=0; bad(){ echo "FAIL: $*"; fail=1; }

# 1. generated SVGs match the master
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
python3 scripts/build-brand.py --svg-only --out "$tmp" >/dev/null || bad "build-brand.py failed"
for f in "$tmp"/logo/*.svg; do
  rel="logo/$(basename "$f")"
  if [ ! -f "brand/$rel" ]; then bad "brand/$rel missing (run scripts/build-brand.py)"
  elif ! cmp -s "$f" "brand/$rel"; then bad "brand/$rel differs from the master (edit tnhc-sign.svg and rebuild; never hand-edit a generated file)"; fi
done

# 2. every icon exists at its size
while read -r rel size; do
  [ -f "brand/$rel" ] || { bad "brand/$rel missing"; continue; }
  got=$(magick identify -format '%wx%h' "brand/$rel" 2>/dev/null || echo none)
  [ "$got" = "${size}x${size}" ] || bad "brand/$rel is $got, expected ${size}x${size}"
done < <(python3 -c '
import importlib.util, sys
spec = importlib.util.spec_from_file_location("b", "scripts/build-brand.py"); b = importlib.util.module_from_spec(spec); spec.loader.exec_module(b)
for rel, s in b.PNGS.items(): print(rel, s)')
if [ -f brand/icons/favicon.ico ]; then
  sizes=$(magick identify -format '%w ' brand/icons/favicon.ico 2>/dev/null | tr ' ' '\n' | sort -n | xargs)
  [ "$sizes" = "16 32 48" ] || bad "brand/icons/favicon.ico holds sizes '$sizes', expected '16 32 48'"
else bad "brand/icons/favicon.ico missing"; fi

# 3. contrast of every colour pair brand.md promises
python3 - <<'PY' || fail=1
def lum(h):
    c = [int(h[i:i + 2], 16) / 255 for i in (1, 3, 5)]
    c = [x / 12.92 if x <= 0.03928 else ((x + 0.055) / 1.055) ** 2.4 for x in c]
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]
def ratio(a, b):
    a, b = lum(a), lum(b)
    return (max(a, b) + 0.05) / (min(a, b) + 0.05)
# (foreground, background, minimum): text 7:1 (AAA), muted and accent 4.5:1 (AA)
PAIRS = [
    ("#EDEDED", "#030303", 7), ("#EDEDED", "#0D0D0D", 7),
    ("#A8A8A8", "#030303", 4.5), ("#A8A8A8", "#0D0D0D", 4.5),
    ("#CCFF00", "#030303", 4.5), ("#CCFF00", "#0D0D0D", 4.5), ("#030303", "#CCFF00", 4.5),
    ("#0A0A0A", "#F4F4F0", 7), ("#0A0A0A", "#FFFFFF", 7),
    ("#55554F", "#F4F4F0", 4.5), ("#55554F", "#FFFFFF", 4.5), ("#0A0A0A", "#CCFF00", 4.5),
]
import os, sys
extra = os.environ.get("BRAND_EXTRA_PAIR")  # test hook: "fg bg min"
if extra:
    fg, bg, m = extra.split(); PAIRS.append((fg, bg, float(m)))
bad = [f"{fg} on {bg} is {ratio(fg, bg):.2f}:1, needs {m}:1" for fg, bg, m in PAIRS if ratio(fg, bg) < m]
for b in bad: print("FAIL: contrast", b)
sys.exit(1 if bad else 0)
PY

# 4. brand.md and the README
if [ -f brand.md ]; then
  for h in "## Logo" "## Colour" "## Typeface" "## Names" "## Files"; do
    grep -qx "$h" brand.md || bad "brand.md lacks the section '$h'"
  done
else bad "brand.md missing"; fi
grep -q 'brand.md' README.md || bad "README.md does not list brand.md"

[ "$fail" = 0 ] && echo PASS || exit 1
```

Write `scripts/tests/check-brand.test.sh` exactly:

```bash
#!/usr/bin/env bash
# Proves check-brand.sh catches each kind of drift, on a throwaway copy.
set -uo pipefail
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
rc=0; ok(){ echo "ok - $1"; }; no(){ echo "NOT OK - $1"; rc=1; }
fresh(){ local d; d=$(mktemp -d); cp -r "$HERE"/. "$d"/; rm -rf "$d/.git"; echo "$d"; }
run(){ ROOT="$1" bash "$1/scripts/check-brand.sh" 2>&1; }

d=$(fresh); out=$(run "$d"); [ $? = 0 ] && ok "clean copy passes" || no "clean copy: $out"; rm -rf "$d"

d=$(fresh); sed -i 's/#EDEDED/#EEEEEE/' "$d/brand/logo/tnhc-lockup-dark.svg"
out=$(run "$d"); [ $? = 1 ] && echo "$out" | grep -q 'tnhc-lockup-dark.svg differs' && ok "hand-edited variant caught" || no "hand-edit: $out"; rm -rf "$d"

d=$(fresh); magick "$d/brand/icons/app-icon-192.png" -resize 100x100 "$d/brand/icons/app-icon-192.png"
out=$(run "$d"); [ $? = 1 ] && echo "$out" | grep -q 'app-icon-192.png is 100x100' && ok "wrong-size icon caught" || no "wrong size: $out"; rm -rf "$d"

d=$(fresh); out=$(BRAND_EXTRA_PAIR="#CCFF00 #F4F4F0 4.5" run "$d"); [ $? = 1 ] && echo "$out" | grep -q 'contrast #CCFF00 on #F4F4F0' && ok "low contrast caught" || no "contrast: $out"; rm -rf "$d"

d=$(fresh); sed -i '/^## Names$/d' "$d/brand.md"
out=$(run "$d"); [ $? = 1 ] && echo "$out" | grep -q "lacks the section '## Names'" && ok "missing section caught" || no "section: $out"; rm -rf "$d"

exit $rc
```

`chmod +x scripts/check-brand.sh scripts/tests/check-brand.test.sh`

- [ ] **Step 2: Run the guard — it must fail**

Run: `cd ~/tnhc-charter-work/handbook && bash scripts/check-brand.sh`
Expected: FAIL lines (`build-brand.py failed`, files missing, `brand.md missing`, `README.md does not list brand.md`).

- [ ] **Step 3: Add the master and the font**

Write `brand/logo/tnhc-sign.svg` exactly:

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 240 216">
  <path id="rim" d="M120 10 L232 204 L8 204 Z" fill="#CCFF00" stroke="#CCFF00" stroke-width="16" stroke-linejoin="round"/>
  <path id="ring" d="M120 26 L218.1 196 L21.9 196 Z" fill="#0A0A0A" stroke="#0A0A0A" stroke-width="10" stroke-linejoin="round"/>
  <path id="field" d="M120 44 L202.5 187 L37.5 187 Z" fill="#CCFF00" stroke="#CCFF00" stroke-width="6" stroke-linejoin="round"/>
  <g id="figure" transform="translate(16.8 25.2) scale(0.86)">
    <g fill="#0A0A0A"><circle cx="120" cy="91" r="16"/><rect x="107" y="110" width="26" height="38" rx="4"/></g>
    <g fill="none" stroke="#0A0A0A" stroke-width="16" stroke-linecap="round"><path d="M114 144 L88 176"/><path d="M126 144 L152 176"/></g>
    <g fill="none" stroke="#0A0A0A" stroke-width="13" stroke-linecap="round" stroke-linejoin="round"><path d="M110 116 L87 108 L108 86"/><path d="M130 116 L153 108 L132 86"/></g>
    <circle cx="120" cy="91" r="16" fill="#0A0A0A"/>
  </g>
</svg>
```

Fetch Figtree Black and its licence at the pinned commit:

```bash
mkdir -p brand/fonts
F=https://raw.githubusercontent.com/erikdkennedy/figtree/032dfa7fe219ef3a02890d6d3add84eacc9aebfe
curl -sfL "$F/fonts/ttf/Figtree-Black.ttf" -o brand/fonts/Figtree-Black.ttf
curl -sfL "$F/OFL.txt" -o brand/fonts/OFL.txt
python3 -c "from fontTools.ttLib import TTFont; f=TTFont('brand/fonts/Figtree-Black.ttf'); print(f['head'].unitsPerEm, f['OS/2'].sCapHeight)"
```
Expected: `1000 700`

- [ ] **Step 4: Write the generator and generate**

Write `scripts/build-brand.py` exactly, then `chmod +x scripts/build-brand.py`:

```python
#!/usr/bin/env python3
"""Generate every TNHC logo file from the one hand-drawn master.

The master is brand/logo/tnhc-sign.svg. Its four layers carry ids (rim, ring,
field, figure); every other file is built from those layers, so a change to the
master is the only way to change the logo.

  python3 scripts/build-brand.py            # SVG variants + PNG/ICO icons
  python3 scripts/build-brand.py --svg-only --out DIR
                                            # SVG variants only, into DIR
                                            # (check-brand.sh diffs these)

SVG output is plain text built in a fixed order, so it is byte-for-byte
reproducible. PNG/ICO go through ImageMagick and are checked by size only.
"""
import argparse, pathlib, subprocess, sys, xml.etree.ElementTree as ET

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
BRAND = ROOT / "brand"
MASTER = BRAND / "logo" / "tnhc-sign.svg"
FONT = BRAND / "fonts" / "Figtree-Black.ttf"

SVGNS = "http://www.w3.org/2000/svg"
VOID, INK, ACID, LIGHT_TEXT = "#030303", "#0A0A0A", "#CCFF00", "#EDEDED"
W, H = 240, 216                     # the master's viewBox
NAME = "THE NO HANDS COMPANY"
CAP = 0.24 * H                      # cap height of the name in the lockup
GAP = 26                            # sign-to-name gap (about one head width)
TRACK = 0.02                        # letter spacing, in em

# Raster outputs: path under brand/ -> pixel size (square).
PNGS = {f"icons/app-icon-{s}.png": s for s in (16, 32, 48, 180, 192, 512)}
PNGS.update({f"icons/android/launcher-{s}.png": s for s in (48, 72, 96, 144, 192)})
PNGS.update({"icons/android/play-store-512.png": 512, "icons/github-avatar-500.png": 500})
ICO = "icons/favicon.ico"           # 16, 32 and 48 inside


def num(v):
    return ("%.2f" % v).rstrip("0").rstrip(".")


def layers():
    ET.register_namespace("", SVGNS)
    tree = ET.parse(MASTER)
    out = {}
    for el in tree.getroot():
        lid = el.get("id")
        if lid:
            out[lid] = ET.tostring(el, encoding="unicode").replace(f' xmlns="{SVGNS}"', "").strip()
    missing = {"rim", "ring", "field", "figure"} - out.keys()
    if missing:
        sys.exit(f"build-brand: master lacks layer(s): {', '.join(sorted(missing))}")
    return out


def recolour(fragment, colour):
    return fragment.replace(ACID, colour).replace(INK, colour)


def svg(view_w, view_h, body):
    return (f'<svg xmlns="{SVGNS}" viewBox="0 0 {num(view_w)} {num(view_h)}">\n'
            f"{body}\n</svg>\n")


def sign(L):
    return "\n".join(L[k] for k in ("rim", "ring", "field", "figure"))


def mono(L):
    # One colour: the ring with the field cut out of it, and the figure.
    field_cut = recolour(L["field"], "#000").replace(' id="field"', "")
    body = (f'<defs><mask id="field-cut" maskUnits="userSpaceOnUse" x="0" y="0" width="{W}" height="{H}">'
            f'<rect width="{W}" height="{H}" fill="#fff"/>{field_cut}</mask></defs>\n'
            f'<g mask="url(#field-cut)">{L["ring"]}</g>\n{L["figure"]}')
    return svg(W, H, body)


def name_path(colour):
    font = TTFont(FONT)
    gs, cmap, hmtx = font.getGlyphSet(), font.getBestCmap(), font["hmtx"]
    s = CAP / font["OS/2"].sCapHeight
    baseline = H / 2 + CAP / 2
    x = W + GAP
    pen = SVGPathPen(gs, ntos=num)
    for i, ch in enumerate(NAME):
        g = cmap[ord(ch)]
        gs[g].draw(TransformPen(pen, (s, 0, 0, -s, x, baseline)))
        x += hmtx[g][0] * s + (TRACK * font["head"].unitsPerEm * s if i < len(NAME) - 1 else 0)
    return f'<path fill="{colour}" d="{pen.getCommands()}"/>', x


def lockup(L, colour):
    text, right = name_path(colour)
    return svg(right, H, sign(L) + "\n" + text)


def app_icon(L, side=256, inner=192):
    k = inner / W
    tx, ty = (side - inner) / 2, (side - H * k) / 2
    body = (f'<rect width="{side}" height="{side}" rx="{num(side * 0.22)}" fill="{VOID}"/>\n'
            f'<g transform="translate({num(tx)} {num(ty)}) scale({num(k)})">\n{sign(L)}\n</g>')
    return svg(side, side, body)


def build_svgs(out):
    L = layers()
    files = {
        "logo/tnhc-sign-mono.svg": mono(L),
        "logo/tnhc-lockup-dark.svg": lockup(L, LIGHT_TEXT),
        "logo/tnhc-lockup-light.svg": lockup(L, INK),
        "logo/tnhc-app-icon.svg": app_icon(L),
    }
    for rel, text in files.items():
        p = out / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
    return files


def magick(*args):
    subprocess.run(["magick", *map(str, args)], check=True)


def build_rasters():
    icon = BRAND / "logo" / "tnhc-app-icon.svg"
    for rel, size in PNGS.items():
        p = BRAND / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        magick("-background", "none", "-density", 600, icon, "-resize", f"{size}x{size}", "-strip", p)
    magick("-background", "none", "-density", 600, icon, "-define", "icon:auto-resize=48,32,16", BRAND / ICO)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--svg-only", action="store_true")
    ap.add_argument("--out", type=pathlib.Path, default=BRAND)
    a = ap.parse_args()
    build_svgs(a.out)
    if not a.svg_only:
        build_rasters()
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

```bash
python3 scripts/build-brand.py
python3 scripts/build-brand.py --svg-only --out /tmp/bb && diff -r brand/logo /tmp/bb/logo
```
Expected: the diff prints only `Only in brand/logo: tnhc-sign.svg` (generation is reproducible).

- [ ] **Step 5: Write the chapter**

`brand.md` exactly:

```markdown
# The No Hands Company Brand

How The No Hands Company looks, in one place. Everything public from TNHC — the website, the apps, the dashboard, the founder's page and the Android app — follows this page. The logo files are in [`brand/`](brand/); see [`brand/README.md`](brand/README.md) for which file to use where. Published at https://tnhc.dev/brand.

## Logo

The logo is a warning sign: a triangle with a green rim, a black ring and a green field, and inside it a figure with its hands behind its head — look, no hands.

- **Use the files, never redraw it.** Every logo file is generated from one master drawing, `brand/logo/tnhc-sign.svg`.
- **Versions:** the full-colour sign; the sign with the name beside it (one file for dark backgrounds, one for light); a one-colour version for print and stamps; and the app icon, the sign on a black tile.
- **Clear space:** leave at least the width of the figure's head (13% of the sign's width) empty on every side.
- **Minimum size:** the sign 16 px wide; the sign with the name 120 px wide.
- **Never:** stretch or rotate it, change its colours beyond the versions above, put text or another picture inside the sign, add shadows, gradients or outlines, or place the full-colour sign on a green background.

## Colour

Acid green, `#CCFF00`, is the TNHC colour. TNHC is dark first: void black is the default background, and every app also offers a light mode that follows your device's setting.

| Role | Dark (default) | Light |
|---|---|---|
| Background | `#030303` | `#F4F4F0` |
| Surface | `#0D0D0D` | `#FFFFFF` |
| Text | `#EDEDED` | `#0A0A0A` |
| Muted text | `#A8A8A8` | `#55554F` |
| Accent | `#CCFF00` — fills and text | `#CCFF00` — fills only, with `#0A0A0A` on top |

- On a light background, acid green is **never** used for text: it is unreadable (1.1:1).
- Every text colour above meets the WCAG AA contrast level (4.5:1) on its background; body text meets AAA (7:1). An automated check holds us to it.

## Typeface

- **Figtree** for everything: headings in Bold or Black, text in Regular, the name in the logo in Black capitals.
- **JetBrains Mono** for code.
- Both are open source under the SIL Open Font License, and we serve them from our own servers, never from a font service.

## Names

- **The No Hands Company** in full the first time on a page, **TNHC** after that. Never "No Hands", "NoHands" or "The No-Hands Company".
- Our apps are called **Nexus** and a plain word: Nexus Chat, Nexus Cloud, Nexus Email. When the maker needs to be clear: "Nexus Chat, by The No Hands Company". Nexus has no logo of its own; the TNHC sign is the only mark.
- **zajfan.tnhc.dev** is the founder's page and looks like the rest of TNHC. The **Zajfan Standard** is TNHC's coding standard, named after its author.
- Our address is written **tnhc.dev**, in lower case.

## Files

| File | Use it for |
|---|---|
| `brand/logo/tnhc-sign.svg` | The sign, anywhere it has room |
| `brand/logo/tnhc-lockup-dark.svg` | The sign with the name, on dark backgrounds |
| `brand/logo/tnhc-lockup-light.svg` | The sign with the name, on light backgrounds |
| `brand/logo/tnhc-sign-mono.svg` | Print, stamps, one-colour uses |
| `brand/logo/tnhc-app-icon.svg` and `brand/icons/` | App icons, the browser tab icon, avatars |

The logo and this page are licensed [CC BY 4.0](LICENSE), like the rest of this handbook; the Charter's [name and logo policy](charter.md#our-name-and-logo) applies. The Figtree font files are under the SIL Open Font License ([`brand/fonts/OFL.txt`](brand/fonts/OFL.txt)).
```

`brand/README.md`:

```markdown
# Brand files

Generated from `logo/tnhc-sign.svg` by `scripts/build-brand.py` — edit the master and rebuild; never edit a generated file. `scripts/check-brand.sh` fails if one drifts.

| Need | File |
|---|---|
| The sign | `logo/tnhc-sign.svg` |
| Sign + name, dark background | `logo/tnhc-lockup-dark.svg` |
| Sign + name, light background | `logo/tnhc-lockup-light.svg` |
| One colour | `logo/tnhc-sign-mono.svg` |
| App icon (vector) | `logo/tnhc-app-icon.svg` |
| App icon PNGs | `icons/app-icon-{16,32,48,180,192,512}.png` |
| Browser tab icon | `icons/favicon.ico` (16, 32, 48) |
| GitHub / social avatar | `icons/github-avatar-500.png` |
| Android launcher | `icons/android/launcher-{48,72,96,144,192}.png`, `icons/android/play-store-512.png` |

Rules for using them: [`../brand.md`](../brand.md).
```

In `README.md`, after the Privacy line, add:

```markdown
3. [Brand](brand.md) — the logo, colour, typeface and names, and the files to use. Published at https://tnhc.dev/brand.
```

- [ ] **Step 6: Run the guards**

```bash
bash scripts/check-brand.sh
bash scripts/tests/check-brand.test.sh
bash scripts/check-charter.sh
```
Expected: `PASS`; five `ok -` lines; `PASS`.

- [ ] **Step 7: Look at it**

Render and inspect (Read the PNGs) — the logo must look right, not merely exist:

```bash
O=$(mktemp -d)
magick -background '#030303' -density 300 brand/logo/tnhc-lockup-dark.svg -resize 700x $O/lockup-dark.png
magick -background '#F4F4F0' -density 300 brand/logo/tnhc-lockup-light.svg -resize 700x $O/lockup-light.png
magick -background '#F4F4F0' -density 300 brand/logo/tnhc-sign-mono.svg -resize 200x $O/mono.png
echo $O
```
Check: name vertically centred on the sign; nothing clipped at any edge; the one-colour version shows ring + figure only; `brand/icons/app-icon-16.png` still shows a triangle with a figure. Record what you saw in the report.

- [ ] **Step 8: Commit**

```bash
git add brand brand.md README.md scripts/build-brand.py scripts/check-brand.sh scripts/tests/check-brand.test.sh
git commit -m "feat: Brand chapter — logo master, generated logo files, guard"
git push origin main
```

---

### Task 2: tnhc.dev from Satoshi to Figtree (licence fix)

**Files:**
- Modify: `scripts/vendor-fonts.sh`, `frontend/public/index.html:30-40`, `frontend/src/index.css:7`, `frontend/tailwind.config.js:11-12`, `design_guidelines.json:10-11`
- Create: `frontend/public/fonts/figtree.css`, `frontend/public/fonts/figtree-{400,500,700,900}.woff2`, `frontend/public/fonts/LICENSE-Figtree.txt`
- Delete: `frontend/public/fonts/satoshi.css`, `frontend/public/fonts/satoshi-{400,500,700}.woff2`, `frontend/public/fonts/LICENSE-Satoshi.txt`
- Create: `scripts/check-fonts.sh`

**Interfaces:**
- Produces: CSS family name `'Figtree'`; Tailwind `font-sans` and `font-heading` both Figtree.

- [ ] **Step 1: Write the check first**

Create `scripts/check-fonts.sh`:

```bash
#!/usr/bin/env bash
# Fails if a font the site may not redistribute is in the tree, if a font has
# no licence file beside it, or if the site fetches fonts from a third party.
set -euo pipefail
cd "$(dirname "$0")/.."
fail=0; bad(){ echo "FAIL: $*"; fail=1; }
hits=$(grep -rli 'satoshi' frontend/src frontend/public frontend/tailwind.config.js design_guidelines.json scripts/vendor-fonts.sh 2>/dev/null | grep -v '^frontend/src/data/' || true)
[ -z "$hits" ] || bad "Satoshi (ITF Free Font License, not redistributable) still referenced: $hits"
ls frontend/public/fonts/*satoshi* >/dev/null 2>&1 && bad "Satoshi font files still present"
for fam in figtree jetbrains-mono; do
  ls frontend/public/fonts/$fam-*.woff2 >/dev/null 2>&1 || bad "$fam font files missing"
done
[ -f frontend/public/fonts/LICENSE-Figtree.txt ] && grep -q 'SIL Open Font License' frontend/public/fonts/LICENSE-Figtree.txt || bad "Figtree licence missing"
grep -rn 'fontshare\|fonts.googleapis\|fonts.gstatic' frontend/public/index.html frontend/src/index.css >/dev/null && bad "a font is fetched from a third party"
[ "$fail" = 0 ] && echo PASS || exit 1
```

(`frontend/src/data/` holds generated blog posts that quote history; they are excluded.)

- [ ] **Step 2: Run — it must fail**

Run: `bash scripts/check-fonts.sh`
Expected: FAIL naming the Satoshi references and files.

- [ ] **Step 3: Vendor Figtree instead of Satoshi**

In `scripts/vendor-fonts.sh`, replace the whole `# ── Satoshi …` section (from its comment line to `rm -f .satoshi-upstream.css`) with:

```bash
# ── Figtree (SIL OFL 1.1) — the TNHC typeface ────────────────────────────────
# From the official repository, pinned, so a refresh is reproducible.
echo "Figtree…"
FIG=https://raw.githubusercontent.com/erikdkennedy/figtree/032dfa7fe219ef3a02890d6d3add84eacc9aebfe
{
  echo "/* Figtree — self-hosted. SIL OFL 1.1; see LICENSE-Figtree.txt."
  echo "   Generated by scripts/vendor-fonts.sh — do not hand-edit. */"
  for pair in 400:Regular 500:Medium 700:Bold 900:Black; do
    w=${pair%%:*}; n=${pair##*:}
    curl -sfL "$FIG/fonts/webfonts/Figtree-$n.woff2" -o "figtree-$w.woff2"
    printf "@font-face {\n  font-family: 'Figtree';\n  src: url('/fonts/figtree-%s.woff2') format('woff2');\n  font-weight: %s;\n  font-style: normal;\n  font-display: swap;\n}\n" "$w" "$w"
  done
} > figtree.css
curl -sfL "$FIG/OFL.txt" -o LICENSE-Figtree.txt
```

Then run the script and remove Satoshi:

```bash
bash scripts/vendor-fonts.sh
git rm -q frontend/public/fonts/satoshi.css frontend/public/fonts/satoshi-400.woff2 frontend/public/fonts/satoshi-500.woff2 frontend/public/fonts/satoshi-700.woff2 frontend/public/fonts/LICENSE-Satoshi.txt
```

- [ ] **Step 4: Point the site at Figtree**

- `frontend/public/index.html`: in the font comment replace `Satoshi (ITF Free Font Licence) and JetBrains Mono (OFL) are` with `Figtree and JetBrains Mono (both SIL OFL) are`; replace `<link rel="stylesheet" href="%PUBLIC_URL%/fonts/satoshi.css" />` with `<link rel="stylesheet" href="%PUBLIC_URL%/fonts/figtree.css" />`. Fix the comment's size figure to the real total (`du -ch frontend/public/fonts/*.woff2 | tail -1`).
- `frontend/src/index.css:7`: `font-family: "Figtree", -apple-system, BlinkMacSystemFont, sans-serif;`
- `frontend/tailwind.config.js`: `heading: ['"Figtree"', 'sans-serif'],` and `sans: ['"Figtree"', 'sans-serif'],`
- `design_guidelines.json`: `"headings": "Figtree"`, `"subheadings": "Figtree"`, `"body": "Figtree"`, and in the rules line `tracking-tighter for headings (Figtree)`.

- [ ] **Step 5: Run the check and the build**

```bash
bash scripts/check-fonts.sh
cd frontend && CI=false bun run build 2>&1 | grep -E 'Compiled|Failed' ; cd ..
grep -o "figtree-[0-9]*\.woff2" frontend/build/fonts/figtree.css | sort -u
```
Expected: `PASS`; `Compiled successfully.`; four woff2 names.

- [ ] **Step 6: Commit and verify live**

```bash
git add -A scripts/vendor-fonts.sh scripts/check-fonts.sh frontend/public/fonts frontend/public/index.html frontend/src/index.css frontend/tailwind.config.js design_guidelines.json
git commit -m "fix: switch to Figtree; Satoshi's licence forbids publishing it in a public repo"
git push origin main
```
Cloudflare Pages deploys on push. Poll (≤10 min) until `curl -s https://tnhc.dev/fonts/figtree.css` returns the `@font-face` block and `curl -s -o /dev/null -w '%{http_code}' https://tnhc.dev/fonts/satoshi.css` is not 200 for the stylesheet (the SPA may answer 200 with index.html — then check the body has no `@font-face`).

---

### Task 3: tnhc.dev/brand

**Files:**
- Create: `frontend/src/pages/Brand.jsx`, `frontend/src/components/site/BrandFiles.jsx`, `frontend/src/lib/brandFiles.js`, `frontend/src/lib/brandFiles.test.js`
- Modify: `frontend/src/App.js` (route), `frontend/src/components/site/SiteFooterLinks.jsx` (link), `scripts/build-sitemap.py:16` (`/brand`), `scripts/webmaster-sync.sh` (generate `brand.js`, copy files)
- Generate: `frontend/src/data/brand.js`, `frontend/public/brand/*`

**Interfaces:**
- Consumes: handbook `brand.md`, `brand/logo/*.svg`, `brand/icons/app-icon-512.png`, `brand/icons/favicon.ico` (Task 1).
- Produces: route `/brand`; footer link `data-testid="footer-brand"`.

- [ ] **Step 1: Write the failing test**

`frontend/src/lib/brandFiles.test.js`:

```js
import { describe, it, expect } from "bun:test";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { BRAND_FILES } from "./brandFiles";

const PUBLIC = join(import.meta.dir, "..", "..", "public");

describe("brand downloads", () => {
  it("every listed file is published under /brand", () => {
    expect(BRAND_FILES.length).toBe(6);
    for (const f of BRAND_FILES) {
      expect(f.href.startsWith("/brand/")).toBe(true);
      expect(existsSync(join(PUBLIC, f.href))).toBe(true);
    }
  });
  it("each file says what it is for and which background it needs", () => {
    for (const f of BRAND_FILES) {
      expect(f.label.length).toBeGreaterThan(0);
      expect(["dark", "light"]).toContain(f.ground);
    }
  });
});
```

Run: `cd frontend && bun test src/lib/brandFiles.test.js`
Expected: FAIL (module not found).

- [ ] **Step 2: List the files**

`frontend/src/lib/brandFiles.js`:

```js
// The logo files offered on /brand. Copied from the handbook by
// scripts/webmaster-sync.sh; "ground" is the background each one is shown on.
export const BRAND_FILES = [
  { href: "/brand/tnhc-sign.svg", label: "The sign", ground: "dark" },
  { href: "/brand/tnhc-lockup-dark.svg", label: "Sign and name, for dark backgrounds", ground: "dark" },
  { href: "/brand/tnhc-lockup-light.svg", label: "Sign and name, for light backgrounds", ground: "light" },
  { href: "/brand/tnhc-sign-mono.svg", label: "One colour, for print", ground: "light" },
  { href: "/brand/tnhc-app-icon.svg", label: "App icon (vector)", ground: "dark" },
  { href: "/brand/app-icon-512.png", label: "App icon, 512 px", ground: "dark" },
];
```

- [ ] **Step 3: Copy the files and generate the page in `webmaster-sync.sh`**

Add `frontend/src/data/brand.js` to the `GEN=(…)` array. After the `privacy.md` generation block, add:

```bash
python3 scripts/build-markdown-page.py --repo "$HANDBOOK" --file brand.md \
    --out frontend/src/data/brand.js --export BRAND \
    --source https://github.com/The-No-Hands-company/handbook/blob/main/brand.md \
    || { echo "FAIL: brand generation" >&2; exit 1; }
mkdir -p frontend/public/brand
cp "$HANDBOOK"/brand/logo/*.svg "$HANDBOOK"/brand/icons/app-icon-512.png "$HANDBOOK"/brand/icons/favicon.ico frontend/public/brand/ \
    || { echo "FAIL: brand files copy" >&2; exit 1; }
```

Run: `HANDBOOK=$HOME/tnhc-charter-work/handbook bash scripts/webmaster-sync.sh /run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems`
Then: `cd frontend && bun test src/lib/brandFiles.test.js` — Expected: PASS (2 tests).

- [ ] **Step 4: The page**

`frontend/src/components/site/BrandFiles.jsx`:

```jsx
import { BRAND_FILES } from "@/lib/brandFiles";

const GROUND = { dark: "bg-[#030303] border-white/15", light: "bg-[#F4F4F0] border-transparent" };

export default function BrandFiles() {
  return (
    <div className="mx-auto mt-28 grid max-w-3xl grid-cols-1 gap-4 px-6 sm:grid-cols-2 md:px-12" data-testid="brand-files">
      {BRAND_FILES.map((f) => (
        <a key={f.href} href={f.href} download
           className="group block border border-white/15 transition-colors hover:border-acid">
          <div className={`flex h-36 items-center justify-center border-b p-6 ${GROUND[f.ground]}`}>
            <img src={f.href} alt={f.label} className="max-h-full max-w-full" />
          </div>
          <div className="px-4 py-3 font-mono text-[12px] text-white/70 group-hover:text-acid">
            {f.label} · {f.href.split(".").pop().toUpperCase()}
          </div>
        </a>
      ))}
    </div>
  );
}
```

`frontend/src/pages/Brand.jsx`:

```jsx
import MarkdownPage from "@/components/site/MarkdownPage";
import BrandFiles from "@/components/site/BrandFiles";
import { BRAND } from "@/data/brand";

export default function Brand() {
  return <MarkdownPage doc={BRAND} testId="brand-page" before={<BrandFiles />} />;
}
```

`frontend/src/App.js`: add `import Brand from "@/pages/Brand";` beside the Privacy import, and `<Route path="/brand" element={<Brand />} />` after the `/privacy` route.

`SiteFooterLinks.jsx`: after the Privacy link and its `{" · "}`, add
`<Link to="/brand" className="transition-colors hover:text-acid" data-testid="footer-brand">Brand</Link>` followed by `{" · "}`.

`scripts/build-sitemap.py:16`: add `"/brand"` after `"/privacy"` in `STATIC`, then run `python3 scripts/build-sitemap.py`.

- [ ] **Step 5: Run the checks and the build**

```bash
cd frontend && bun test src/lib/ && CI=false bun run build 2>&1 | grep -E 'Compiled|Failed'; cd ..
bash scripts/check-fonts.sh && bash scripts/check-no-donation-ask.sh
grep -c 'tnhc.dev/brand' frontend/public/sitemap.xml
```
Expected: all bun tests pass; `Compiled successfully.`; `PASS` twice; `1`.

- [ ] **Step 6: Commit and verify live**

```bash
git add frontend/src/pages/Brand.jsx frontend/src/components/site/BrandFiles.jsx frontend/src/lib/brandFiles.js frontend/src/lib/brandFiles.test.js frontend/src/App.js frontend/src/components/site/SiteFooterLinks.jsx scripts/build-sitemap.py scripts/webmaster-sync.sh frontend/src/data frontend/public/brand frontend/public/sitemap.xml
git commit -m "feat: tnhc.dev/brand — the Brand chapter and its logo files"
git push origin main
```
Poll (≤10 min) until `https://tnhc.dev/brand` returns 200 and `curl -s -o /dev/null -w '%{http_code} %{content_type}' https://tnhc.dev/brand/tnhc-sign.svg` is `200 image/svg+xml`.

---

### Task 4: Final verification and hand-off

**Files:** none changed (report only).

- [ ] **Step 1: Guards**

```bash
cd ~/tnhc-charter-work/handbook && bash scripts/check-brand.sh && bash scripts/tests/check-brand.test.sh && bash scripts/check-charter.sh
cd /run/media/zajferx/Data/dev/The-No-hands-Company/tnhc.dev && bash scripts/check-fonts.sh && bash scripts/check-no-donation-ask.sh
```
Expected: every line PASS / `ok -`.

- [ ] **Step 2: Live**

- `https://tnhc.dev/brand` 200; each of the six `/brand/…` files 200 with an image content type.
- `https://tnhc.dev/charter` and `/privacy` still 200.
- The live CSS bundle names `Figtree` and not `Satoshi`: `js=$(curl -s https://tnhc.dev/ | grep -o '/static/css/main\.[a-f0-9]*\.css' | head -1); curl -s https://tnhc.dev$js | grep -c Figtree; curl -s https://tnhc.dev$js | grep -ci satoshi` → `≥1` and `0`.

- [ ] **Step 3: Inspect the logo once more**

Render `brand/icons/github-avatar-500.png` and `brand/logo/tnhc-lockup-light.svg` and Read them. Record that they look right.

- [ ] **Step 4: Hand-off note for the founder**

Report the GitHub avatar file path (`~/tnhc-charter-work/handbook/brand/icons/github-avatar-500.png`) and these steps: open https://github.com/organizations/The-No-Hands-company/settings/profile → under "Profile picture" click "Upload new picture" → choose the file → "Set new profile picture".
