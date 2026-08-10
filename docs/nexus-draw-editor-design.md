# Nexus-Draw — Editor Excellence (Phase 1) Design Spec

**Status:** Approved for build, 2026-08-10.
**Scope:** Turn Nexus-Draw's facade into a genuinely good single-user infinite-canvas
whiteboard / diagramming editor, shipped to `draw.tnhc.dev`. Stays a **static site,
$0, no accounts, no backend.** Real-time collaboration is **phase 2** (pre-committed,
not in this spec).

---

## 1. Current state (what we're fixing)
`apps/Nexus-Draw/frontend` has a real skeleton — a WebGL2 canvas (pan/zoom/grid), a
Zustand store with an element/layer/undo-redo model (undo/redo already implemented
and history-pushed by `addElement`/`updateElement`/`removeElement`/`reorderElements`),
and panels (Toolbar, LayerPanel, PropertiesPanel, TopBar). But the renderer **draws
only rectangles**, so the 12 advertised tools don't actually work, and the panels are
inert. Backend (Python) + engine (Rust) exist but are **not deployed**; only the
static frontend is live. Stack: React 19, Vite, zustand 5, tailwind, lucide-react.
`yjs`/`y-websocket` are already deps (phase-2 groundwork, unused in phase 1).

## 2. Rendering
- **Replace the WebGL2 rect-only renderer with Canvas 2D.** Standard for this genre
  (Excalidraw, tldraw) because text, freehand, dashed/rounded strokes, arrows, and
  hit-testing are trivial in 2D and brutal in raw WebGL.
- **Two render styles, per element:** `clean` (native Canvas 2D paths) and `sketch`
  (via **`roughjs`** — Excalidraw's engine). Freehand pen via **`perfect-freehand`**
  (pressure/velocity-aware strokes).
- Core is a pure `renderElement(ctx, el, viewport)` that branches on
  `el.style.styleMode`. Every element carries a stable integer `seed` so `sketch`
  shapes don't re-wobble each frame.
- New deps: `roughjs`, `perfect-freehand` (runtime); `vitest`, `jsdom` (dev/test).

## 3. Element & style model
Extend the existing `ElementData` (keep `id`, `elementType`, `data`, `style`,
`transform`, `order`). Concretely:
- `elementType`: `"rectangle" | "ellipse" | "line" | "arrow" | "freehand" | "text" | "sticky" | "image"`.
- `data` per type: rect/ellipse `{x,y,width,height,radius?}`; line/arrow `{x1,y1,x2,y2}`;
  freehand `{points: [x,y,pressure][]}`; text `{x,y,text,width?}`; sticky `{x,y,width,height,text}`;
  image `{x,y,width,height,src}` (data URL).
- `style` (all honored): `stroke`, `fill` (`"none"` allowed), `strokeWidth`,
  `strokeStyle` (`solid|dashed|dotted`), `opacity`, `radius`, `fontFamily`, `fontSize`,
  `textAlign`, and **`styleMode` (`clean|sketch`) per element**, defaulting to the
  board's `defaultStyleMode`. `seed:number` lives on the element.
- `board` gains `defaultStyleMode` and `gridSnap:boolean`.

## 4. Tools & interactions (all real)
- Tools: `select`, `hand` (pan), `pen`, `rectangle`, `ellipse`, `line`, `arrow`,
  `text`, `sticky`, `eraser`. (Drop the fake `fill`/`zoom` toolbar buttons; zoom is
  wheel/keys, fill is a style.)
- Select: click hit-test + marquee, multi-select (shift), **move, resize (8 handles),
  rotate**; delete; copy/paste/duplicate; z-order nudge.
- Keyboard shortcuts (V/H/P/R/E/L/A/T/S, Del, Ctrl+Z/Y/C/V/D, Space-pan) + a
  shortcuts overlay (`?`).
- Snap-to-grid toggle; existing infinite pan/zoom/grid retained.

## 5. Panels (wired)
- **PropertiesPanel:** edits the selected element(s)' full style set incl. the
  clean/sketch toggle; multi-select edits apply to all.
- **LayerPanel:** real z-reorder (drag or up/down), rename, hide, lock, delete.
- **Undo/redo:** wire the existing store stacks to toolbar buttons + Ctrl+Z/Y.

## 6. Persistence & export
- **Local autosave** to `localStorage` (debounced); new / rename / clear board;
  **import/export board `.json`**. No backend, no accounts — stays static.
- **Export PNG** (raster; current viewport or whole-board bounds) and **export SVG**
  (vector; `renderElementSvg(el)` mirrors the clean/sketch branch, roughjs emits SVG
  paths for sketch).

## 7. File structure
```
frontend/src/
  components/Canvas/Canvas.tsx     # canvas el, pointer + viewport handling (replaces WebGL)
  render/renderElement.ts          # clean + sketch draw to a 2D ctx (roughjs, perfect-freehand)
  render/renderElementSvg.ts       # same, emitting SVG for export
  render/hitTest.ts                # point-in-element, marquee, resize/rotate handle geometry
  render/geometry.ts               # bounds, transforms, arrow heads (pure)
  tools/toolController.ts          # maps pointer events → element create/edit per active tool
  stores/useEditorStore.ts         # extend model + selectors; history already wired
  export/exportPng.ts  export/exportSvg.ts
  persistence/localStore.ts        # serialize/deserialize + autosave
  components/{Toolbar,PropertiesPanel,LayerPanel,TopBar}.tsx  # wired
```

## 8. Testing approach (honest)
- **Unit-tested (vitest, added):** `geometry.ts`, `hitTest.ts`, `renderElementSvg.ts`
  (assert emitted SVG strings), `persistence` serialize/deserialize round-trip,
  store mutations (add/update/remove/reorder/undo/redo), style resolution
  (element vs board default).
- **Not unit-tested (visual/build verification):** the imperative `renderElement` to
  a live 2D context and pointer interactions — verified by building + running the app
  (screenshot at the edge after deploy). Canvas pixel output is not meaningfully
  unit-testable; the pure geometry/model/SVG paths that feed it are, and they carry
  the real logic.

## 9. Ship
`bun run build` (frontend) → redeploy `draw.tnhc.dev` via the Hosting deploy flow
already used (create/upload/deploy through the API with the existing `fh_` token).
Verify at the edge: index + assets 200, tools work, export works.

## 10. Non-goals (phase 1)
Real-time collaboration, accounts, server persistence, the Python/Rust backends, AI
assist, connectors that auto-route between shapes. All phase 2+. `yjs` stays a
dormant dep.
