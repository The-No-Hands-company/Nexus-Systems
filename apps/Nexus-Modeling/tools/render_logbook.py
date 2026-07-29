#!/usr/bin/env python3
"""Render docs/kernel-logbook.md as the designed HTML book edition.

The logbook is the narrative record of the kernel (see the logbook convention: Parts
are bodies of work, Chapters are feature arcs, passages are increments). This script
is the reproducible source of the 'book treatment' edition -- warm paper and prussian
ink with brass accents, a book serif, light and dark, and a reading-progress rule --
so that regenerating it after adding a chapter is one deterministic command rather
than a hand-built artifact.

    python3 tools/render_logbook.py            # -> docs/kernel-logbook.html
    python3 tools/render_logbook.py --check    # exit 1 if the output is stale

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
        sink = chapter["blocks"] if chapter else doc.preamble
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
            part = {"title": title, "id": slug(title), "chapters": []}
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
                part = {"title": "", "id": "front", "chapters": []}
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


CSS = """
:root{
  --paper:#f7f3ea; --paper-edge:#efe8da; --ink:#1b2733; --ink-soft:#4d5c6b;
  --rule:#d9cfba; --brass:#9a7328; --brass-soft:#bf9743;
  --quote:#2d3f52; --code-bg:#ece5d6;
}
:root[data-theme="dark"]{
  --paper:#141a21; --paper-edge:#10151b; --ink:#dfe6ee; --ink-soft:#9fb0c0;
  --rule:#2a3542; --brass:#d8ab55; --brass-soft:#b98f3f; --quote:#c8d6e4;
  --code-bg:#1c242d;
}
*{box-sizing:border-box}
html{scroll-behavior:smooth}
body{
  margin:0; background:var(--paper); color:var(--ink);
  font-family:"Iowan Old Style","Palatino Linotype",Palatino,"Book Antiqua",Georgia,serif;
  font-size:19px; line-height:1.72; -webkit-font-smoothing:antialiased;
}
#progress{position:fixed;top:0;left:0;height:3px;width:0;background:var(--brass);z-index:50}
.wrap{max-width:44rem;margin:0 auto;padding:0 1.6rem 6rem}
header.book{padding:5rem 0 2.5rem;border-bottom:1px solid var(--rule);margin-bottom:3rem}
.mark{font-size:2rem;line-height:1;margin-bottom:1.2rem}
h1{font-size:2.5rem;line-height:1.15;margin:0 0 1rem;letter-spacing:-.015em;font-weight:600}
.subtitle em,.subtitle{color:var(--ink-soft);font-size:1.05rem}
.subtitle p{margin:0}
nav.toc{margin:0 0 3.5rem;padding:1.6rem 1.8rem;background:var(--paper-edge);
  border:1px solid var(--rule);border-radius:3px}
nav.toc h2{font-size:.75rem;text-transform:uppercase;letter-spacing:.14em;
  color:var(--brass);margin:0 0 1rem;font-weight:700;font-family:system-ui,sans-serif}
nav.toc ol{list-style:none;margin:0;padding:0}
nav.toc .part{font-size:.82rem;text-transform:uppercase;letter-spacing:.1em;
  font-family:system-ui,sans-serif;color:var(--ink-soft);margin:1.1rem 0 .45rem;font-weight:600}
nav.toc .part:first-child{margin-top:0}
nav.toc li a{display:block;padding:.16rem 0;color:var(--ink);text-decoration:none;font-size:.97rem}
nav.toc li a:hover{color:var(--brass);text-decoration:underline}
nav.toc .n{color:var(--brass-soft);font-variant-numeric:tabular-nums;margin-right:.5rem}
section.part>h2{font-family:system-ui,sans-serif;font-size:.8rem;text-transform:uppercase;
  letter-spacing:.16em;color:var(--brass);font-weight:700;
  margin:4.5rem 0 0;padding-bottom:.7rem;border-bottom:2px solid var(--brass-soft)}
article.chapter{margin:3rem 0 0}
article.chapter h3{font-size:1.55rem;line-height:1.25;margin:0 0 1.4rem;font-weight:600;
  letter-spacing:-.01em;scroll-margin-top:2rem}
article.chapter h3 .n{color:var(--brass-soft);font-variant-numeric:tabular-nums;margin-right:.55rem}
p{margin:0 0 1.35rem;text-align:justify;hyphens:auto}
blockquote{margin:2.2rem 0;padding:0 0 0 1.6rem;border-left:3px solid var(--brass-soft)}
blockquote p{margin:0;font-style:italic;font-size:1.14rem;color:var(--quote);text-align:left}
.proven{margin:2rem 0;padding:1.3rem 1.5rem;background:var(--paper-edge);
  border:1px solid var(--rule);border-left:3px solid var(--brass);border-radius:2px;
  font-size:.97rem;text-align:left}
.proven-label{display:block;font-family:system-ui,sans-serif;font-size:.7rem;
  text-transform:uppercase;letter-spacing:.14em;color:var(--brass);font-weight:700;
  margin-bottom:.55rem}
code{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-size:.87em;
  background:var(--code-bg);padding:.1em .38em;border-radius:3px}
.coda{margin-top:4rem;padding-top:2rem;border-top:1px solid var(--rule);
  color:var(--ink-soft);font-size:1rem}
.coda p{text-align:left;margin:0}
#theme{position:fixed;top:1.1rem;right:1.1rem;z-index:60;width:2.4rem;height:2.4rem;
  border-radius:50%;border:1px solid var(--rule);background:var(--paper-edge);
  color:var(--ink);cursor:pointer;font-size:1rem;line-height:1;display:grid;place-items:center}
#theme:hover{border-color:var(--brass)}
@media (max-width:640px){body{font-size:17.5px}h1{font-size:2rem}.wrap{padding:0 1.15rem 4rem}
  p{text-align:left}}
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


def render(doc: Doc) -> str:
    toc: list[str] = []
    for part in doc.parts:
        if part["title"]:
            toc.append(
                f'<li class="part">{inline(part["title"])}</li>'
            )
        for ch in part["chapters"]:
            n = f'<span class="n">{ch["num"]}</span>' if ch["num"] else ""
            toc.append(f'<li><a href="#{ch["id"]}">{n}{inline(ch["title"])}</a></li>')

    body: list[str] = []
    for part in doc.parts:
        body.append(f'<section class="part" id="{part["id"]}">')
        if part["title"]:
            body.append(f'<h2>{inline(part["title"])}</h2>')
        for ch in part["chapters"]:
            n = f'<span class="n">{ch["num"]}.</span>' if ch["num"] else ""
            body.append(f'<article class="chapter" id="{ch["id"]}">')
            body.append(f'<h3>{n}{inline(ch["title"])}</h3>')
            body.extend(ch["blocks"])
            body.append("</article>")
        body.append("</section>")

    nl = "\n"
    return f"""<!DOCTYPE html>
<html lang="en" data-theme="light">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(doc.title)}</title>
<meta name="description" content="The narrative record of building the Nexus geometry kernel.">
<link rel="icon" href="{FAVICON}">
<style>{CSS}</style>
</head>
<body>
<div id="progress"></div>
<button id="theme" aria-label="Toggle light and dark">&#9790;</button>
<div class="wrap">
<header class="book">
  <div class="mark">&#128214;</div>
  <h1>{inline(doc.title)}</h1>
  <div class="subtitle">{doc.subtitle}</div>
</header>
{nl.join(doc.preamble)}
<nav class="toc"><h2>Contents</h2><ol>
{nl.join(toc)}
</ol></nav>
{nl.join(body)}
<div class="coda">{doc.coda}</div>
</div>
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
    chapters = sum(len(p["chapters"]) for p in doc.parts)

    if args.check:
        if not OUT.exists() or OUT.read_text(encoding="utf-8") != out:
            print(f"stale: regenerate with `python3 {Path(__file__).name}`", file=sys.stderr)
            return 1
        print(f"up to date ({chapters} chapters)")
        return 0

    OUT.write_text(out, encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} — {len(doc.parts)} parts, {chapters} chapters, "
          f"{len(out)/1024:.1f} KiB")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
