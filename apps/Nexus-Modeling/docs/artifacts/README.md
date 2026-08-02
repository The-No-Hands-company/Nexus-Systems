# Published artifact sources

Each file here is the **source of a published Artifact** — the page content only, with
no `<!doctype>` / `<html>` / `<head>` / `<body>`, because the Artifact host supplies the
document shell and its own light/dark toggle.

| File | Published at |
|---|---|
| `modeling-parity-index.html` | https://claude.ai/code/artifact/5f636e76-5d79-4fa3-8b9e-85ebd819790d |
| `kernel-foundation-inspection.html` | https://claude.ai/code/artifact/64aff1d1-b386-4d9c-9737-24b9f4fe1c39 |
| `nexus-roadmap.html` | https://claude.ai/code/artifact/bb7ac986-266e-448e-98b7-d7fa7bb33bf9 |

The **Logbook** is not here because it is generated, not hand-maintained:
`tools/render_logbook.py` renders `docs/kernel-logbook.md` into both
`docs/kernel-logbook.html` (standalone) and `docs/kernel-logbook.artifact.html`
(published at `.../artifact/3dadb64a-…`). Both are written on every run and
`--check` fails if either is behind the markdown.

## Why these exist

These pages went weeks without an update — the Logbook artifact sat 44 chapters behind
its own markdown — because they were hand-built in a chat and nothing was on disk.
Updating one meant re-fetching the published HTML and patching it by hand, which is the
kind of step that quietly stops happening. Keeping the source in the repo makes a
refresh an ordinary edit.

## Theme

The host stamps `data-theme="light"` / `"dark"` on the root element and that must beat
`prefers-color-scheme` **in both directions** — so define palette tokens on `:root`,
override them under `@media (prefers-color-scheme: dark)`, then override again under
BOTH `:root[data-theme="dark"]` and `:root[data-theme="light"]`. Omitting the explicit
light rule leaves a reader who forces light mode on a dark-mode machine reading the
dark palette.
