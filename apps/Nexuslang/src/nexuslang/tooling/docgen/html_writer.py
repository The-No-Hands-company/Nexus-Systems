"""Self-contained HTML documentation writer for NexusLang projects.

Produces:
- ``index.html``   — module listing with search bar
- ``<module>.html`` for every source file that has documented items
- ``search_index.json`` — search data consumed by client-side JS in the pages

All output is self-contained (no external CDN dependencies).  A dark/light
theme toggle is included via inline CSS custom properties.
"""

from __future__ import annotations

import html
import json
import re
from pathlib import Path
from typing import Dict, List, Optional

from .extractor import DocItem


# ── helpers ─────────────────────────────────────────────────────────────────


def _esc(text: str) -> str:
    return html.escape(text or "")


def _slug(text: str) -> str:
    """Convert text to a URL/anchor-safe slug."""
    return re.sub(r"[^a-zA-Z0-9_-]", "-", text).strip("-").lower()


def _highlight_nxl(code: str) -> str:
    """
    Minimal syntax highlighting for NexusLang code snippets.
    Adds ``<span class="kw">`` / ``<span class="str">`` / ``<span class="cmt">``
    wrappers using simple regex substitution.  Operates on raw (unescaped) text
    and returns HTML.
    """
    KEYWORDS = (
        r"\b(function|async|class|struct|enum|trait|interface|module|"
        r"if|else|while|for|each|in|return|end|set|to|with|and|or|not|"
        r"is|as|print|text|import|export|extend|implements|override|"
        r"new|null|true|false|do|default|returns|type|of|address|"
        r"dereference|sizeof|allocate|free|list)\b"
    )

    segments: List[str] = []
    pos = 0

    # Tokenise via regex — order matters (strings & comments first)
    pattern = re.compile(
        r'(##[^\n]*)'    # doc comment
        r'|(#[^\n]*)'    # regular comment
        r'|("(?:[^"\\]|\\.)*")'  # double-quoted string
        r"|('(?:[^'\\]|\\.)*')"  # single-quoted string
    )

    for m in pattern.finditer(code):
        # Flush plain text before the match
        plain = code[pos:m.start()]
        if plain:
            plain = _esc(plain)
            plain = re.sub(KEYWORDS, r'<span class="kw">\1</span>', plain)
            segments.append(plain)

        matched = m.group(0)
        if matched.startswith("##"):
            segments.append(f'<span class="cmt">{_esc(matched)}</span>')
        elif matched.startswith("#"):
            segments.append(f'<span class="cmt">{_esc(matched)}</span>')
        else:
            segments.append(f'<span class="str">{_esc(matched)}</span>')

        pos = m.end()

    # Flush remaining text
    tail = code[pos:]
    if tail:
        tail = _esc(tail)
        tail = re.sub(KEYWORDS, r'<span class="kw">\1</span>', tail)
        segments.append(tail)

    return "".join(segments)


# ── CSS / JS (inlined) ───────────────────────────────────────────────────────

_SHARED_CSS = """\
:root{
  --bg:#ffffff;--bg2:#f5f5f7;--fg:#1d1d1f;--fg2:#6e6e73;
  --accent:#0071e3;--border:#d2d2d7;--code-bg:#f3f3f6;
  --kw:#0070c1;--str:#c41a16;--cmt:#5c6b7a;
}
[data-theme=dark]{
  --bg:#1c1c1e;--bg2:#2c2c2e;--fg:#f5f5f7;--fg2:#98989d;
  --accent:#2997ff;--border:#3a3a3c;--code-bg:#2c2c2e;
  --kw:#79c0ff;--str:#f0a070;--cmt:#8b949e;
}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
  background:var(--bg);color:var(--fg);line-height:1.6}
a{color:var(--accent);text-decoration:none}a:hover{text-decoration:underline}
header{background:var(--bg2);border-bottom:1px solid var(--border);
  padding:0 2rem;display:flex;align-items:center;gap:1rem;height:3.5rem;position:sticky;top:0;z-index:100}
header .brand{font-weight:700;font-size:1.1rem}
header .spacer{flex:1}
#search-input{padding:.35rem .75rem;border:1px solid var(--border);border-radius:6px;
  background:var(--bg);color:var(--fg);font-size:.9rem;width:18rem}
#theme-btn{background:none;border:1px solid var(--border);border-radius:6px;
  padding:.3rem .6rem;cursor:pointer;color:var(--fg);font-size:.85rem}
main{max-width:1100px;margin:2rem auto;padding:0 2rem}
h1{font-size:1.8rem;font-weight:700;margin-bottom:1rem}
h2{font-size:1.3rem;font-weight:600;margin:2rem 0 .6rem;padding-top:2rem;
  border-top:1px solid var(--border)}
h3{font-size:1.05rem;font-weight:600;margin:.8rem 0 .3rem}
.badge{display:inline-block;font-size:.72rem;font-weight:600;padding:.15rem .4rem;
  border-radius:4px;margin-right:.4rem;vertical-align:middle}
.badge-fn{background:#dbeafe;color:#1e40af}[data-theme=dark] .badge-fn{background:#1e3a5f;color:#93c5fd}
.badge-class{background:#dcfce7;color:#166534}[data-theme=dark] .badge-class{background:#14532d;color:#86efac}
.badge-struct{background:#fef3c7;color:#92400e}[data-theme=dark] .badge-struct{background:#451a03;color:#fcd34d}
.badge-enum{background:#ede9fe;color:#5b21b6}[data-theme=dark] .badge-enum{background:#2e1065;color:#c4b5fd}
.badge-trait{background:#fce7f3;color:#9d174d}[data-theme=dark] .badge-trait{background:#500724;color:#f9a8d4}
.badge-interface{background:#e0f2fe;color:#075985}[data-theme=dark] .badge-interface{background:#0c4a6e;color:#7dd3fc}
.badge-deprecated{background:#fee2e2;color:#991b1b}[data-theme=dark] .badge-deprecated{background:#450a0a;color:#fca5a5}
.item-card{border:1px solid var(--border);border-radius:10px;padding:1.2rem 1.4rem;margin-bottom:1.2rem;transition:border-color .15s}
.item-card:hover{border-color:var(--accent)}
.item-title{display:flex;align-items:center;gap:.4rem;flex-wrap:wrap}
.item-name{font-weight:700;font-size:1rem}
.item-desc{color:var(--fg);margin:.4rem 0}
.item-section{margin-top:.7rem}
.item-section-label{font-size:.8rem;text-transform:uppercase;letter-spacing:.05em;color:var(--fg2);font-weight:600;margin-bottom:.3rem}
table.params{width:100%;border-collapse:collapse;font-size:.9rem}
table.params th{text-align:left;color:var(--fg2);font-weight:600;padding:.3rem .6rem;border-bottom:1px solid var(--border)}
table.params td{padding:.3rem .6rem;vertical-align:top;border-bottom:1px solid var(--border)}
td.param-name{font-family:monospace;font-size:.88rem}
pre.example{background:var(--code-bg);border:1px solid var(--border);border-radius:8px;
  padding:1rem;overflow-x:auto;font-family:"Fira Code","JetBrains Mono",monospace;font-size:.87rem;line-height:1.5}
pre.example .kw{color:var(--kw);font-weight:600}
pre.example .str{color:var(--str)}
pre.example .cmt{color:var(--cmt);font-style:italic}
.returns-val{font-size:.92rem;color:var(--fg)}
.module-list{list-style:none;padding:0}
.module-list li{padding:.4rem 0;border-bottom:1px solid var(--border)}
.module-list li:last-child{border:none}
.toc{background:var(--bg2);border-radius:8px;padding:1rem 1.2rem;margin-bottom:2rem;font-size:.9rem}
.toc ul{list-style:none;padding:0;columns:3;column-gap:1.5rem}
.toc li{padding:.15rem 0}
#search-results{position:absolute;top:3.5rem;right:2rem;width:22rem;max-height:60vh;overflow-y:auto;
  background:var(--bg);border:1px solid var(--border);border-radius:8px;box-shadow:0 8px 30px rgba(0,0,0,.15);z-index:200;display:none}
.sr-item{padding:.55rem .9rem;cursor:pointer;font-size:.88rem}
.sr-item:hover{background:var(--bg2)}
.sr-kind{font-size:.72rem;color:var(--fg2);margin-left:.3rem}
.sr-file{font-size:.72rem;color:var(--fg2)}
footer{text-align:center;color:var(--fg2);font-size:.8rem;padding:2rem;border-top:1px solid var(--border);margin-top:4rem}
"""

_SHARED_JS = """\
(function(){
  var btn=document.getElementById('theme-btn');
  var saved=localStorage.getItem('nlpl-doc-theme');
  if(saved){document.documentElement.setAttribute('data-theme',saved);}
  if(btn){btn.addEventListener('click',function(){
    var cur=document.documentElement.getAttribute('data-theme');
    var next=cur==='dark'?'light':'dark';
    document.documentElement.setAttribute('data-theme',next);
    localStorage.setItem('nlpl-doc-theme',next);
  });}

  /* search */
  var inp=document.getElementById('search-input');
  var res=document.getElementById('search-results');
  if(!inp||!res)return;
  var idx=null;
  function loadIdx(){
    if(idx!==null)return Promise.resolve();
    return fetch('search_index.json').then(function(r){return r.json();}).then(function(d){idx=d;});
  }
  inp.addEventListener('focus',function(){loadIdx();});
  inp.addEventListener('input',function(){
    var q=(inp.value||'').toLowerCase().trim();
    if(!q||!idx){res.style.display='none';return;}
    var hits=idx.filter(function(e){
      return e.name.toLowerCase().includes(q)||e.description.toLowerCase().includes(q);
    }).slice(0,12);
    if(!hits.length){res.style.display='none';return;}
    res.innerHTML=hits.map(function(e){
      return '<div class="sr-item" onclick="location.href=\\''+e.file+'#'+e.anchor+'\\'">'+
        '<span>'+escHtml(e.name)+'</span>'+
        '<span class="sr-kind">'+escHtml(e.kind)+'</span>'+
        '<div class="sr-file">'+escHtml(e.file)+' &mdash; '+escHtml(e.description.slice(0,80))+'</div>'+
      '</div>';
    }).join('');
    res.style.display='block';
  });
  document.addEventListener('click',function(ev){
    if(!res.contains(ev.target)&&ev.target!==inp)res.style.display='none';
  });
  function escHtml(s){var d=document.createElement('div');d.textContent=s;return d.innerHTML;}
})();
"""


# ── per-item HTML rendering ──────────────────────────────────────────────────

def _badge(kind: str) -> str:
    label = kind.capitalize()
    return f'<span class="badge badge-{kind}">{label}</span>'


def _render_item(item: DocItem) -> str:
    parts: List[str] = []
    anchor = _slug(item.name)
    parts.append(f'<div class="item-card" id="{anchor}">')
    parts.append(f'  <div class="item-title">')
    parts.append(f'    {_badge(item.kind)}')
    if item.is_deprecated:
        parts.append(f'    <span class="badge badge-deprecated">Deprecated</span>')
    parts.append(f'    <span class="item-name">{_esc(item.name)}</span>')
    parts.append(f'  </div>')

    if item.description:
        parts.append(f'  <p class="item-desc">{_esc(item.description)}</p>')

    if item.is_deprecated and item.deprecated:
        parts.append(f'  <p class="item-desc"><strong>Deprecated:</strong> {_esc(item.deprecated)}</p>')

    if item.params:
        parts.append('  <div class="item-section">')
        parts.append('    <div class="item-section-label">Parameters</div>')
        parts.append('    <table class="params"><tr><th>Name</th><th>Description</th></tr>')
        for p in item.params:
            parts.append(f'    <tr><td class="param-name">{_esc(p.name)}</td><td>{_esc(p.description)}</td></tr>')
        parts.append('    </table>')
        parts.append('  </div>')

    if item.returns:
        parts.append('  <div class="item-section">')
        parts.append('    <div class="item-section-label">Returns</div>')
        parts.append(f'    <p class="returns-val">{_esc(item.returns)}</p>')
        parts.append('  </div>')

    for idx, example in enumerate(item.examples, 1):
        label = "Example" if len(item.examples) == 1 else f"Example {idx}"
        parts.append('  <div class="item-section">')
        parts.append(f'    <div class="item-section-label">{label}</div>')
        highlighted = _highlight_nxl(example)
        parts.append(f'    <pre class="example">{highlighted}</pre>')
        parts.append('  </div>')

    if item.see_also:
        parts.append('  <div class="item-section">')
        parts.append('    <div class="item-section-label">See Also</div>')
        links = ", ".join(
            f'<a href="#{_slug(ref)}">{_esc(ref)}</a>' for ref in item.see_also
        )
        parts.append(f'    <p>{links}</p>')
        parts.append('  </div>')

    parts.append('</div>')
    return "\n".join(parts)


# ── page builders ────────────────────────────────────────────────────────────

def _wrap_page(title: str, body: str, depth: int = 0) -> str:
    """Wrap body content in a full HTML document."""
    rel = ("../" * depth) if depth else "./"
    search_idx_path = rel + "search_index.json"
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{_esc(title)}</title>
<style>
{_SHARED_CSS}
</style>
</head>
<body>
<header>
  <span class="brand">NLPL Docs</span>
  <span class="spacer"></span>
  <input id="search-input" type="search" placeholder="Search..." autocomplete="off">
  <button id="theme-btn">Theme</button>
</header>
<div id="search-results"></div>
<main>
{body}
</main>
<footer>Generated by <strong>NLPL Doc Generator</strong></footer>
<script>
const _SEARCH_INDEX_URL = '{search_idx_path}';
{_SHARED_JS}
</script>
</body>
</html>"""


def _build_module_page(rel_path: str, items: List[DocItem]) -> str:
    """Render the HTML content for a single module page."""
    module_name = rel_path.replace("\\", "/").replace(".nxl", "")
    body_parts: List[str] = []
    body_parts.append(f"<h1>{_esc(module_name)}</h1>")

    # Table of contents
    if len(items) > 3:
        body_parts.append('<nav class="toc">')
        body_parts.append("  <strong>Contents</strong>")
        body_parts.append("  <ul>")
        for item in items:
            anchor = _slug(item.name)
            body_parts.append(f'    <li><a href="#{anchor}">{_esc(item.name)}</a></li>')
        body_parts.append("  </ul>")
        body_parts.append("</nav>")

    for item in items:
        body_parts.append(_render_item(item))

    return _wrap_page(f"{module_name} — NexusLang Docs", "\n".join(body_parts))


def _build_index_page(pkg_name: str, pkg_version: str, items_by_file: Dict[str, List[DocItem]]) -> str:
    """Render the main index page."""
    all_items = [item for items in items_by_file.values() for item in items]
    fn_count = sum(1 for i in all_items if i.kind == "function")
    cls_count = sum(1 for i in all_items if i.kind in ("class", "struct", "trait", "interface", "enum"))

    body_parts: List[str] = []
    body_parts.append(f'<h1>{_esc(pkg_name)} <small style="font-size:.7em;font-weight:400;color:var(--fg2)">v{_esc(pkg_version)}</small></h1>')
    body_parts.append(
        f'<p style="color:var(--fg2);margin-bottom:1.5rem">'
        f'{len(items_by_file)} module{"s" if len(items_by_file) != 1 else ""} &mdash; '
        f'{fn_count} function{"s" if fn_count != 1 else ""} &mdash; '
        f'{cls_count} type{"s" if cls_count != 1 else ""}</p>'
    )

    if not items_by_file:
        body_parts.append("<p>No documented items found.</p>")
    else:
        body_parts.append("<h2>Modules</h2>")
        body_parts.append('<ul class="module-list">')
        for rel_path, items in sorted(items_by_file.items()):
            module_name = rel_path.replace("\\", "/").replace(".nxl", "")
            page_href = _page_filename(rel_path)
            kinds = sorted({i.kind for i in items})
            kind_badges = "".join(_badge(k) for k in kinds)
            body_parts.append(
                f'  <li><a href="{_esc(page_href)}">{_esc(module_name)}</a>'
                f'  {kind_badges}'
                f'  <span style="color:var(--fg2);font-size:.85rem"> — {len(items)} item{"s" if len(items)!=1 else ""}</span>'
                f'</li>'
            )
        body_parts.append("</ul>")

    return _wrap_page(f"{pkg_name} API Reference", "\n".join(body_parts))


def _page_filename(rel_path: str) -> str:
    """Convert a .nlpl relative path to an HTML filename."""
    name = rel_path.replace("\\", "/").replace("/", "_").replace(".nxl", "")
    return f"{_slug(name)}.html"


def _build_search_index(items_by_file: Dict[str, List[DocItem]]) -> List[Dict]:
    index = []
    for rel_path, items in items_by_file.items():
        page = _page_filename(rel_path)
        for item in items:
            index.append({
                "name": item.name,
                "kind": item.kind,
                "file": page,
                "anchor": _slug(item.name),
                "description": item.description[:200],
            })
    return index


# ── public API ───────────────────────────────────────────────────────────────

def generate_html(
    items_by_file: Dict[str, List[DocItem]],
    output_dir: str,
    pkg_name: str = "NLPL Project",
    pkg_version: str = "0.1.0",
) -> List[Path]:
    """Write documentation HTML files to *output_dir*.

    Args:
        items_by_file: Mapping from source-relative path to list of
                       :class:`~nlpl.tooling.docgen.extractor.DocItem`.
        output_dir:    Directory to write HTML and JSON files into.
        pkg_name:      Name of the project (used in page titles).
        pkg_version:   Version string (displayed on the index page).

    Returns:
        List of :class:`pathlib.Path` objects for every file written.
    """
    out = Path(output_dir)
    out.mkdir(parents=True, exist_ok=True)

    written: List[Path] = []

    # index.html
    index_html = _build_index_page(pkg_name, pkg_version, items_by_file)
    index_path = out / "index.html"
    index_path.write_text(index_html, encoding="utf-8")
    written.append(index_path)

    # Per-module pages
    for rel_path, items in items_by_file.items():
        page_name = _page_filename(rel_path)
        page_path = out / page_name
        page_html = _build_module_page(rel_path, items)
        page_path.write_text(page_html, encoding="utf-8")
        written.append(page_path)

    # search_index.json
    search_idx = _build_search_index(items_by_file)
    idx_path = out / "search_index.json"
    idx_path.write_text(json.dumps(search_idx, ensure_ascii=False, indent=2), encoding="utf-8")
    written.append(idx_path)

    return written
