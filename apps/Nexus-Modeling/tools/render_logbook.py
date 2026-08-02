#!/usr/bin/env python3
"""Render docs/kernel-logbook.md as the designed HTML book edition.

The logbook is the narrative record of the kernel (see the logbook convention: Parts
are bodies of work, Chapters are feature arcs, passages are increments). This script
is the reproducible source of the 'book treatment' edition -- warm paper and prussian
ink with brass accents, a book serif, light and dark, and a reading-progress rule --
so that regenerating it after adding a chapter is one deterministic command rather
than a hand-built artifact.

    python3 tools/render_logbook.py            # -> docs/kernel-logbook{,.artifact}.html
    python3 tools/render_logbook.py --check    # exit 1 if either build is stale

Two builds, always written together: the standalone file, and a body-only build for
publishing as a hosted Artifact (which supplies its own document shell and theme
control). They are never written separately, because the Artifact drifting behind the
book is exactly what happens when refreshing it is a step someone has to remember.

Stdlib only, on purpose: it must run in any checkout with no install step.
"""

from __future__ import annotations

import argparse
import html
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "docs" / "kernel-logbook.md"
OUT = ROOT / "docs" / "kernel-logbook.html"
# Body-only build for the hosted Artifact, which supplies its own document shell.
OUT_ARTIFACT = ROOT / "docs" / "kernel-logbook.artifact.html"

# ── inline markdown ──────────────────────────────────────────────────────────────
# Applied to already-escaped text, so the tags we emit are the only markup present.
INLINE = (
    (re.compile(r"`([^`]+)`"), r"<code>\1</code>"),
    (re.compile(r"\*\*([^*]+)\*\*"), r"<strong>\1</strong>"),
    (re.compile(r"(?<![*\w])\*([^*\n]+)\*(?!\*)"), r"<em>\1</em>"),
)


def inline(text: str) -> str:
    out = html.escape(text, quote=False)
    for pattern, repl in INLINE:
        out = pattern.sub(repl, out)
    return out


def slug(text: str) -> str:
    s = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return s or "section"


class Doc:
    """The parsed logbook: a title, a preamble, and a run of Parts holding Chapters."""

    def __init__(self) -> None:
        self.title = "The Nexus Geometry Kernel — A Logbook"
        self.subtitle = ""
        self.preamble: list[str] = []   # rendered blocks before the first Part
        self.parts: list[dict] = []     # {title, id, chapters: [{num, title, id, blocks}]}
        self.coda = ""


def parse(md: str) -> Doc:
    doc = Doc()
    lines = md.split("\n")

    # Drop the machine-authored Contents section; the rendered edition builds its own
    # from the actual headings, so a stale hand-written list cannot drift.
    body: list[str] = []
    skipping = False
    for line in lines:
        if line.strip() == "## Contents":
            skipping = True
            continue
        if skipping:
            if line.startswith("#") or line.strip() == "---":
                skipping = False
            else:
                continue
        body.append(line)

    part: dict | None = None
    chapter: dict | None = None
    para: list[str] = []
    quote: list[str] = []

    def flush() -> None:
        """Emit any buffered paragraph or blockquote into the current sink."""
        nonlocal para, quote
        # A paragraph after a Part heading but before its first Chapter is that Part's
        # OWN introduction. Routing it to the preamble instead — which is what happened
        # until this was noticed — piled all five Part introductions at the top of the
        # book, so the reader met every Part's opening before reaching Part I.
        if chapter is not None:
            sink = chapter["blocks"]
        elif part is not None:
            sink = part["blurb"]
        else:
            sink = doc.preamble
        if para:
            text = " ".join(para).strip()
            if text:
                if text.startswith("**What was proven.**"):
                    inner = inline(text[len("**What was proven.**"):].strip())
                    sink.append(
                        '<aside class="proven"><span class="proven-label">'
                        "What was proven</span>" + inner + "</aside>"
                    )
                else:
                    sink.append("<p>" + inline(text) + "</p>")
            para = []
        if quote:
            sink.append(
                '<blockquote><p>' + inline(" ".join(quote).strip()) + "</p></blockquote>"
            )
            quote = []

    for raw in body:
        line = raw.rstrip()
        stripped = line.strip()

        if stripped.startswith("> "):
            flush() if para else None
            quote.append(stripped[2:])
            continue
        if quote and not stripped.startswith("> "):
            flush()

        if not stripped or stripped == "---":
            flush()
            continue

        if stripped.startswith("# Part "):
            flush()
            chapter = None
            title = stripped[2:]
            part = {"title": title, "id": slug(title), "chapters": [], "blurb": []}
            doc.parts.append(part)
            continue

        if stripped.startswith("# "):          # document title
            flush()
            doc.title = stripped[2:]
            continue

        if stripped.startswith("## "):
            flush()
            head = stripped[3:]
            m = re.match(r"^(\d+)\.\s*(.+)$", head)
            num, ctitle = (m.group(1), m.group(2)) if m else ("", head)
            if part is None:  # a chapter before any Part heading
                part = {"title": "", "id": "front", "chapters": [], "blurb": []}
                doc.parts.append(part)
            chapter = {
                "num": num,
                "title": ctitle,
                "id": slug(f"{num}-{ctitle}"),
                "blocks": [],
            }
            part["chapters"].append(chapter)
            continue

        para.append(stripped)

    flush()

    # The opening italic line is the book's subtitle; the closing one is its coda.
    if doc.preamble and doc.preamble[0].startswith("<p><em>"):
        doc.subtitle = doc.preamble.pop(0)
    last_chapter = doc.parts[-1]["chapters"][-1] if doc.parts and doc.parts[-1]["chapters"] else None
    if last_chapter and last_chapter["blocks"] and last_chapter["blocks"][-1].startswith("<p><em>"):
        doc.coda = last_chapter["blocks"].pop()
    return doc


# The book's palette, kept in one place so the two output modes cannot drift apart.
LIGHT_TOKENS = """
  --paper:#F4F2EC; --card:#FBFAF5; --ink:#1A1D21; --ink-soft:#3C4046; --grey:#6E6A62;
  --rule:#D8D3C7; --rule-soft:#E7E2D6;
  --prussian:#2B4C6F; --brass:#9C7A2E; --code-bg:#E7E2D6;
  --serif:"Iowan Old Style","Palatino Linotype","Book Antiqua",Palatino,Georgia,"Times New Roman",serif;
  --mono:ui-monospace,"SF Mono","Cascadia Code",Menlo,Consolas,monospace;
"""
DARK_TOKENS = """
  --paper:#16181C; --card:#1C1F24; --ink:#E9E4D8; --ink-soft:#C6C1B4; --grey:#928C7E;
  --rule:#2C2F35; --rule-soft:#23262B;
  --prussian:#8FB2D6; --brass:#D8AE57; --code-bg:#23262B;
"""

# Standalone file: the page owns its theme, chosen once on load and stored.
THEME_STANDALONE = f':root{{{LIGHT_TOKENS}}}\n:root[data-theme="dark"]{{{DARK_TOKENS}}}'

# Artifact: the HOST owns the theme. It stamps data-theme on the root element, and that
# has to beat the OS preference in BOTH directions — hence the explicit light override,
# which is easy to forget and leaves a reader who forces light mode staring at the dark
# palette on a dark-mode machine.
THEME_ARTIFACT = (
    f':root{{{LIGHT_TOKENS}}}\n'
    f'@media (prefers-color-scheme: dark){{:root{{{DARK_TOKENS}}}}}\n'
    f':root[data-theme="dark"]{{{DARK_TOKENS}}}\n'
    f':root[data-theme="light"]{{{LIGHT_TOKENS}}}'
)

CSS = """
*{box-sizing:border-box}
html{scroll-behavior:smooth}
body{
  margin:0; background:var(--paper); color:var(--ink);
  font-family:var(--serif);
  font-size:1.155rem; line-height:1.72; -webkit-font-smoothing:antialiased;
  text-rendering:optimizeLegibility;
}
#progress{position:fixed;top:0;left:0;height:2px;width:0;background:var(--brass);
  z-index:50;transition:width .08s linear}
.wrap{max-width:41rem;margin:0 auto;padding:0 1.5rem 5rem}
p{margin:0 0 1.1rem;hyphens:auto}
code{font-family:var(--mono);font-size:.86em;background:var(--code-bg);
  padding:.06em .36em;border-radius:3px;color:var(--ink)}
a{color:var(--prussian);text-decoration:none;
  border-bottom:1px solid color-mix(in srgb,var(--prussian) 34%,transparent)}
a:hover{border-bottom-color:var(--prussian)}
a:focus-visible{outline:2px solid var(--brass);outline-offset:3px;border-radius:2px}

/* ── Title page ─────────────────────────────────────────────────────────── */
header.book{min-height:86vh;display:flex;flex-direction:column;justify-content:center;
  padding:4rem 0 3rem;border-bottom:1px solid var(--rule)}
.mark{font-size:1.6rem;line-height:1;margin:0 0 1.4rem}
h1{font-size:clamp(2.4rem,6.5vw,3.7rem);line-height:1.05;margin:0;font-weight:600;
  letter-spacing:-.012em;text-wrap:balance}
.title-rule{width:3.4rem;height:2px;background:var(--prussian);margin:1.5rem 0}
.subtitle,.subtitle em{color:var(--ink-soft);font-size:1.2rem;font-style:italic}
.subtitle p{margin:0;max-width:33rem}
.frontmatter{margin-top:2.4rem;border-top:1px solid var(--rule-soft);padding-top:1.2rem}
.frontmatter p{font-family:var(--mono);font-size:.75rem;line-height:1.9;letter-spacing:.02em;
  color:var(--grey);max-width:34rem;margin:0 0 .5rem}
.frontmatter strong{color:var(--ink-soft);font-weight:600}

/* ── Contents ───────────────────────────────────────────────────────────── */
nav.toc{margin:3.2rem 0;padding:2rem 0;border-top:1px solid var(--rule);
  border-bottom:1px solid var(--rule)}
nav.toc h2{font-family:var(--mono);font-size:.72rem;letter-spacing:.3em;text-transform:uppercase;
  color:var(--grey);margin:0 0 1.5rem;font-weight:400}
nav.toc ol{list-style:none;margin:0;padding:0}
nav.toc li.part{font-size:1rem;font-weight:600;color:var(--prussian);
  margin:1.5rem 0 .5rem;letter-spacing:.004em}
nav.toc li.part:first-child{margin-top:0}
nav.toc li.ch{display:flex;align-items:baseline;gap:.5rem;margin:.26rem 0;font-size:.99rem}
nav.toc li.ch .n{font-family:var(--mono);font-size:.78rem;color:var(--brass);
  min-width:1.6rem;font-variant-numeric:tabular-nums}
nav.toc li.ch .t{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;color:var(--ink-soft)}
nav.toc li.ch .dots{flex:1;border-bottom:1px dotted var(--rule);transform:translateY(-.18em);
  min-width:1rem}
nav.toc li.ch a{border:0;color:inherit}
nav.toc li.ch a:hover{color:var(--prussian)}

/* ── Part divider ───────────────────────────────────────────────────────── */
section.part>.divider{text-align:center;padding:5rem 0 3rem;margin-top:1.5rem}
section.part>.divider .pn{font-family:var(--mono);font-size:.74rem;letter-spacing:.34em;
  text-transform:uppercase;color:var(--brass)}
section.part>.divider h2{font-size:clamp(1.65rem,4.4vw,2.2rem);font-weight:600;
  margin:.75rem 0 1.1rem;letter-spacing:-.01em;text-wrap:balance}
section.part>.divider .blurb p{font-style:italic;color:var(--ink-soft);font-size:1.06rem;
  max-width:32rem;margin:0 auto .8rem;text-align:left}
section.part>.divider .orn{width:2.2rem;height:1px;background:var(--prussian);margin:1.5rem auto 0}

/* ── Chapter ────────────────────────────────────────────────────────────── */
article.chapter{padding:2.5rem 0 1.4rem;scroll-margin-top:1.5rem}
article.chapter+article.chapter{border-top:1px solid var(--rule-soft)}
.ch-head{display:flex;align-items:flex-start;gap:1.1rem;margin-bottom:1.3rem}
.ch-num{font-size:3rem;line-height:.9;color:var(--prussian);font-weight:600;
  font-variant-numeric:lining-nums;flex-shrink:0;opacity:.9}
.ch-titles{padding-top:.2rem}
.ch-kicker{font-family:var(--mono);font-size:.67rem;letter-spacing:.24em;text-transform:uppercase;
  color:var(--brass);margin:0 0 .32rem}
h3.ch-title{font-size:1.58rem;font-weight:600;line-height:1.16;margin:0;
  letter-spacing:-.006em;text-wrap:balance}
article.chapter>p:first-of-type::first-letter{initial-letter:2.4;font-weight:600;
  color:var(--prussian);margin-right:.55rem}

blockquote{margin:1.6rem 0;padding:.2rem 0 .2rem 1.3rem;border-left:2px solid var(--brass)}
blockquote p{margin:0;font-size:1.26rem;line-height:1.42;font-style:italic;
  color:var(--ink);font-weight:500;text-wrap:pretty}

.proven{margin:1.5rem 0 .6rem;padding:.95rem 1.15rem;background:var(--card);
  border:1px solid var(--rule);border-radius:5px}
.proven-label{display:block;font-family:var(--mono);font-size:.65rem;letter-spacing:.22em;
  text-transform:uppercase;color:var(--brass);margin-bottom:.35rem}
.proven{font-size:.98rem;color:var(--ink-soft);line-height:1.62}

.coda{text-align:center;font-style:italic;color:var(--grey);max-width:33rem;
  margin:2.5rem auto 4rem;padding-top:2.3rem;border-top:1px solid var(--rule);font-size:1.04rem}
.coda p{margin:0}

#theme{position:fixed;top:1.1rem;right:1.1rem;z-index:60;width:2.4rem;height:2.4rem;
  border-radius:50%;border:1px solid var(--rule);background:var(--card);
  color:var(--ink);cursor:pointer;font-size:1rem;line-height:1;display:grid;place-items:center}
#theme:hover{border-color:var(--brass)}

@media (max-width:520px){
  body{font-size:1.08rem}
  .ch-head{gap:.8rem}.ch-num{font-size:2.3rem}
  nav.toc li.ch .t{white-space:normal}
  .wrap{padding:0 1.15rem 4rem}
}
@media (prefers-reduced-motion:reduce){#progress{transition:none}}
@media print{#theme,#progress,nav.toc{display:none}body{background:#fff;color:#000}}
"""

JS = """
(function(){
  var KEY='nexus-logbook-theme';
  var root=document.documentElement;
  var saved=null; try{saved=localStorage.getItem(KEY);}catch(e){}
  if(!saved){saved=window.matchMedia&&window.matchMedia('(prefers-color-scheme: dark)').matches?'dark':'light';}
  root.setAttribute('data-theme',saved);
  var btn=document.getElementById('theme');
  function paint(){btn.textContent=root.getAttribute('data-theme')==='dark'?'\\u2600':'\\u263D';}
  paint();
  btn.addEventListener('click',function(){
    var next=root.getAttribute('data-theme')==='dark'?'light':'dark';
    root.setAttribute('data-theme',next);
    try{localStorage.setItem(KEY,next);}catch(e){}
    paint();
  });
  var bar=document.getElementById('progress');
  function scroll(){
    var h=document.documentElement.scrollHeight-window.innerHeight;
    bar.style.width=(h>0?(window.scrollY/h)*100:0)+'%';
  }
  scroll(); window.addEventListener('scroll',scroll,{passive:true});
  window.addEventListener('resize',scroll);
})();
"""

FAVICON = (
    "data:image/svg+xml,"
    "%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E"
    "%3Ctext y='.9em' font-size='90'%3E%F0%9F%93%96%3C/text%3E%3C/svg%3E"
)


# Artifact build: the reading-progress rule only. The host renders the theme switch, so
# duplicating it here would put two controls on one page that disagree with each other.
JS_ARTIFACT = """
(function(){
  var bar=document.getElementById('progress');
  if(!bar) return;
  function scroll(){
    var h=document.documentElement.scrollHeight-window.innerHeight;
    bar.style.width=(h>0?(window.scrollY/h)*100:0)+'%';
  }
  scroll(); window.addEventListener('scroll',scroll,{passive:true});
  window.addEventListener('resize',scroll);
})();
"""

def render(doc: Doc, artifact: bool = False) -> str:
    def kicker(part_title: str) -> str:
        """'Part IV — Closing every gap' -> 'Closing every gap' (drop any subtitle)."""
        t = part_title.split("—", 1)[1].strip() if "—" in part_title else part_title
        return t.split(":", 1)[0].strip()

    def numeral(i: int) -> str:
        words = ["One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten"]
        return words[i] if i < len(words) else str(i + 1)

    toc: list[str] = []
    for part in doc.parts:
        if part["title"]:
            toc.append(f'<li class="part">{inline(part["title"])}</li>')
        for ch in part["chapters"]:
            n = f'<span class="n">{ch["num"]}</span>' if ch["num"] else '<span class="n"></span>'
            toc.append(
                f'<li class="ch">{n}'
                f'<span class="t"><a href="#{ch["id"]}">{inline(ch["title"])}</a></span>'
                f'<span class="dots"></span></li>'
            )

    body: list[str] = []
    for idx, part in enumerate(doc.parts):
        body.append(f'<section class="part" id="{part["id"]}">')
        if part["title"]:
            body.append('<div class="divider">')
            body.append(f'<div class="pn">Part {numeral(idx)}</div>')
            body.append(f'<h2>{inline(kicker(part["title"]))}</h2>')
            if part["blurb"]:
                body.append('<div class="blurb">' + "\n".join(part["blurb"]) + "</div>")
            body.append('<div class="orn"></div>')
            body.append("</div>")
        kick = inline(kicker(part["title"])) if part["title"] else ""
        for ch in part["chapters"]:
            body.append(f'<article class="chapter" id="{ch["id"]}">')
            body.append('<div class="ch-head">')
            if ch["num"]:
                body.append(f'<div class="ch-num">{ch["num"]}</div>')
            body.append('<div class="ch-titles">')
            if kick:
                body.append(f'<p class="ch-kicker">{kick}</p>')
            body.append(f'<h3 class="ch-title">{inline(ch["title"])}</h3>')
            body.append("</div></div>")
            body.extend(ch["blocks"])
            body.append("</article>")
        body.append("</section>")

    nl = "\n"
    page = f"""<div class="wrap">
<header class="book">
  <div class="mark">&#128214;</div>
  <h1>{inline(doc.title)}</h1>
  <div class="title-rule"></div>
  <div class="subtitle">{doc.subtitle}</div>
  <div class="frontmatter">{nl.join(doc.preamble)}</div>
</header>
<nav class="toc"><h2>Contents</h2><ol>
{nl.join(toc)}
</ol></nav>
{nl.join(body)}
<div class="coda">{doc.coda}</div>
</div>"""

    if artifact:
        # The host supplies <!doctype>, <head> and <body>, and its own theme control — so
        # this emits page content only, and drops the book's own toggle button rather than
        # putting a second, competing switch on the same page.
        return f"""<title>{html.escape(doc.title)}</title>
<style>{THEME_ARTIFACT}{CSS}</style>
<div id="progress"></div>
{page}
<script>{JS_ARTIFACT}</script>
"""

    return f"""<!DOCTYPE html>
<html lang="en" data-theme="light">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(doc.title)}</title>
<meta name="description" content="The narrative record of building the Nexus geometry kernel.">
<link rel="icon" href="{FAVICON}">
<style>{THEME_STANDALONE}{CSS}</style>
</head>
<body>
<div id="progress"></div>
<button id="theme" aria-label="Toggle light and dark">&#9790;</button>
{page}
<script>{JS}</script>
</body>
</html>
"""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="do not write; exit 1 if the output is missing or stale")
    args = ap.parse_args()

    if not SRC.exists():
        print(f"error: {SRC} not found", file=sys.stderr)
        return 2

    doc = parse(SRC.read_text(encoding="utf-8"))
    out = render(doc)
    art = render(doc, artifact=True)
    chapters = sum(len(p["chapters"]) for p in doc.parts)

    if args.check:
        # BOTH builds. The published Artifact fell 14 chapters behind precisely because
        # refreshing it was a separate step someone had to remember; a staleness check that
        # only looked at the standalone file would have kept saying "up to date".
        for path, want in ((OUT, out), (OUT_ARTIFACT, art)):
            if not path.exists() or path.read_text(encoding="utf-8") != want:
                print(f"stale: {path.name} — regenerate with `python3 {Path(__file__).name}`",
                      file=sys.stderr)
                return 1
        print(f"up to date ({chapters} chapters, both builds)")
        return 0

    OUT.write_text(out, encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} — {len(doc.parts)} parts, {chapters} chapters, "
          f"{len(out)/1024:.1f} KiB")
    OUT_ARTIFACT.write_text(art, encoding="utf-8")
    print(f"wrote {OUT_ARTIFACT.relative_to(ROOT)} — {len(art)/1024:.1f} KiB "
          f"(this is the one to publish as an Artifact)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
