# Nexus-Draw Editor (Phase 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Nexus-Draw a genuinely usable single-user infinite-canvas whiteboard/diagramming editor (Canvas 2D, clean + sketch styles, all tools real, export, local save) and redeploy it to draw.tnhc.dev.

**Architecture:** Replace the WebGL rect-only renderer with a Canvas 2D renderer that branches per-element on `styleMode` (clean = native paths, sketch = roughjs; freehand = perfect-freehand). Pure geometry/hit-test/SVG modules carry the logic and are unit-tested; the imperative canvas draw and pointer interactions are verified by building and running. Zustand store (undo/redo already wired) is extended with the real element model.

**Tech Stack:** React 19, Vite, zustand 5, tailwind, lucide-react, roughjs, perfect-freehand; vitest + jsdom for tests. All work under `apps/Nexus-Draw/frontend`.

## Global Constraints

- Work dir: `apps/Nexus-Draw/frontend`. Nexus-Draw is a normal monorepo directory (not a submodule) — commits are plain monorepo commits.
- Build must stay green: `bun run build` (= `tsc && vite build`) and `bun run check` (`tsc --noEmit`). TypeScript strict.
- Tests: `npx vitest run` (added in Task 1).
- Styling: tailwind classes, existing dark `zinc` theme. Icons: `lucide-react`.
- Keep the store's existing history model — `addElement/updateElement/removeElement/reorderElements` already call `pushHistory()`; do not double-push.
- `styleMode` on an element overrides the board's `defaultStyleMode`; a resolver returns the effective mode.
- No backend, no network, no accounts in phase 1. Persistence is `localStorage` only.
- Commit after every green step; conventional-commit messages.

---

## Task 1: Deps, test setup, and the element/style model

**Files:**
- Modify: `package.json` (deps + test script), create `vitest.config.ts`
- Modify: `src/stores/useEditorStore.ts` (types + `defaultStyleMode`, `gridSnap`, `resolveStyleMode`)
- Create: `src/stores/model.ts` (shared types + factories), `src/stores/model.test.ts`

**Interfaces:**
- Produces:
  - `type StyleMode = "clean" | "sketch"`
  - `type ElementType = "rectangle"|"ellipse"|"line"|"arrow"|"freehand"|"text"|"sticky"|"image"`
  - `interface ElementStyle { stroke:string; fill:string; strokeWidth:number; strokeStyle:"solid"|"dashed"|"dotted"; opacity:number; radius:number; fontFamily:string; fontSize:number; textAlign:"left"|"center"|"right"; styleMode?:StyleMode }`
  - `interface ElementData { id:string; elementType:ElementType; data:Record<string,any>; style:ElementStyle; transform:{a,b,c,d,e,f}; order:number; seed:number }`
  - `function makeElement(type:ElementType, data:object, style?:Partial<ElementStyle>): ElementData`
  - `function resolveStyleMode(el:ElementData, boardDefault:StyleMode): StyleMode`
  - store gains `board.defaultStyleMode`, `board.gridSnap`.

- [ ] **Step 1: Add deps and test script**

```bash
cd apps/Nexus-Draw/frontend
npm i roughjs perfect-freehand
npm i -D vitest jsdom @testing-library/react
```
Add to `package.json` scripts: `"test": "vitest run"`.

- [ ] **Step 2: Create `vitest.config.ts`**

```ts
import { defineConfig } from "vitest/config";
export default defineConfig({ test: { environment: "jsdom", globals: true } });
```

- [ ] **Step 3: Write the failing model test**

```ts
// src/stores/model.test.ts
import { describe, it, expect } from "vitest";
import { makeElement, resolveStyleMode, type ElementData } from "./model";

describe("makeElement", () => {
  it("creates a rectangle with defaults and a stable seed", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
    expect(el.elementType).toBe("rectangle");
    expect(el.style.stroke).toBeTypeOf("string");
    expect(el.style.strokeWidth).toBeGreaterThan(0);
    expect(el.seed).toBeTypeOf("number");
    expect(el.id).toMatch(/.+/);
  });
});

describe("resolveStyleMode", () => {
  it("uses the element override when set", () => {
    const el = makeElement("rectangle", {}, { styleMode: "sketch" });
    expect(resolveStyleMode(el, "clean")).toBe("sketch");
  });
  it("falls back to the board default when unset", () => {
    const el = makeElement("rectangle", {});
    delete el.style.styleMode;
    expect(resolveStyleMode(el, "sketch")).toBe("sketch");
  });
});
```

- [ ] **Step 4: Run to verify it fails** — `npx vitest run src/stores/model.test.ts` → FAIL (module missing).

- [ ] **Step 5: Implement `src/stores/model.ts`**

```ts
export type StyleMode = "clean" | "sketch";
export type ElementType = "rectangle"|"ellipse"|"line"|"arrow"|"freehand"|"text"|"sticky"|"image";
export interface ElementStyle {
  stroke:string; fill:string; strokeWidth:number;
  strokeStyle:"solid"|"dashed"|"dotted"; opacity:number; radius:number;
  fontFamily:string; fontSize:number; textAlign:"left"|"center"|"right"; styleMode?:StyleMode;
}
export interface ElementData {
  id:string; elementType:ElementType; data:Record<string,any>;
  style:ElementStyle; transform:{a:number;b:number;c:number;d:number;e:number;f:number}; order:number; seed:number;
}
const DEFAULT_STYLE: ElementStyle = {
  stroke:"#e4e4e7", fill:"none", strokeWidth:2, strokeStyle:"solid",
  opacity:1, radius:8, fontFamily:"ui-sans-serif, system-ui", fontSize:20, textAlign:"left",
};
export function makeElement(type:ElementType, data:Record<string,any>, style:Partial<ElementStyle>={}):ElementData {
  return {
    id: crypto.randomUUID(), elementType:type, data,
    style: { ...DEFAULT_STYLE, ...style },
    transform: { a:1,b:0,c:0,d:1,e:0,f:0 }, order: 0,
    seed: Math.floor(Math.random()*2**31),
  };
}
export function resolveStyleMode(el:ElementData, boardDefault:StyleMode):StyleMode {
  return el.style.styleMode ?? boardDefault;
}
```

- [ ] **Step 6: Wire the store to `model.ts`** — replace the local `ElementData`/`Vec2` interfaces in `useEditorStore.ts` with imports from `./model`; add `defaultStyleMode:StyleMode` and `gridSnap:boolean` to `BoardData`; add a `defaultStyleMode` selector default of `"clean"`.

- [ ] **Step 7: Run tests + typecheck** — `npx vitest run && bun run check` → PASS / no TS errors.

- [ ] **Step 8: Commit** — `git add -A && git commit -m "feat(draw): element/style model, per-element styleMode, vitest setup"`

---

## Task 2: Pure geometry + hit-testing

**Files:** Create `src/render/geometry.ts`, `src/render/hitTest.ts`, `src/render/hitTest.test.ts`

**Interfaces:**
- Consumes: `ElementData` (Task 1).
- Produces:
  - `function elementBounds(el:ElementData): {x:number;y:number;width:number;height:number}`
  - `function hitElement(el:ElementData, p:{x:number;y:number}, tol:number): boolean`
  - `function hitInMarquee(el:ElementData, rect:{x,y,width,height}): boolean`
  - `function resizeHandles(b:{x,y,width,height}): {id:string;x:number;y:number}[]` (8 handles + rotate)
  - `function arrowHead(x1,y1,x2,y2,size): {p1:[number,number];p2:[number,number]}`

- [ ] **Step 1: Write failing tests**

```ts
// src/render/hitTest.test.ts
import { describe, it, expect } from "vitest";
import { elementBounds, hitElement, hitInMarquee } from "./hitTest";
import { makeElement } from "../stores/model";

const rect = makeElement("rectangle", { x:10, y:10, width:100, height:50 });

describe("elementBounds", () => {
  it("returns the rect's box", () => {
    expect(elementBounds(rect)).toEqual({ x:10, y:10, width:100, height:50 });
  });
});
describe("hitElement", () => {
  it("hits inside the rect", () => expect(hitElement(rect, {x:50,y:30}, 4)).toBe(true));
  it("misses outside", () => expect(hitElement(rect, {x:500,y:500}, 4)).toBe(false));
});
describe("hitInMarquee", () => {
  it("is selected when fully inside the marquee", () => {
    expect(hitInMarquee(rect, {x:0,y:0,width:200,height:200})).toBe(true);
  });
  it("is not selected when the marquee misses it", () => {
    expect(hitInMarquee(rect, {x:300,y:300,width:50,height:50})).toBe(false);
  });
});
```

- [ ] **Step 2: Run → FAIL.** `npx vitest run src/render/hitTest.test.ts`

- [ ] **Step 3: Implement `geometry.ts` then `hitTest.ts`.** `elementBounds` switches on `elementType` (rect/ellipse/sticky/text/image from `data.x/y/width/height`; line/arrow from min/max of endpoints; freehand from point extents). `hitElement`: for filled/box types point-in-rect with `tol`; for line/arrow distance-to-segment ≤ `tol+strokeWidth`; for freehand min distance to any segment. `hitInMarquee`: bounds fully contained. `resizeHandles`: 8 box handles + one rotate handle above top-center. `arrowHead`: two points from the end angle.

- [ ] **Step 4: Run → PASS.**
- [ ] **Step 5: Commit** — `git commit -m "feat(draw): pure geometry + hit-testing"`

---

## Task 3: Canvas 2D renderer (replaces WebGL) — clean + sketch + freehand

**Files:** Create `src/render/renderElement.ts`; rewrite `src/components/Canvas/Canvas.tsx`.

**Interfaces:**
- Consumes: `ElementData`, `resolveStyleMode` (T1), `elementBounds`, `arrowHead` (T2), `roughjs`, `perfect-freehand`.
- Produces:
  - `function renderElement(ctx:CanvasRenderingContext2D, rc:RoughCanvas, el:ElementData, mode:StyleMode): void`
  - `Canvas.tsx` draws all elements each frame into a DPR-scaled 2D context with pan/zoom applied, plus grid and selection overlay.

- [ ] **Step 1: Implement `renderElement.ts`** — one function; `mode==="sketch"` uses `rc.rectangle/ellipse/line/linearPath` with `{ seed: el.seed, roughness, stroke, fill, strokeWidth, fillStyle:"hachure" }`; `mode==="clean"` uses native `ctx` paths (`roundRect`, `ellipse`, `moveTo/lineTo`, dashed via `setLineDash`). `freehand` always uses `perfect-freehand`'s `getStroke(points)` → fill a `Path2D`. `text` uses `ctx.fillText` with the style font. `arrow` = line + `arrowHead` triangle. Apply `opacity` via `ctx.globalAlpha`.

- [ ] **Step 2: Rewrite `Canvas.tsx`** — replace all WebGL with a 2D context: on resize set canvas to `clientWidth*dpr`; each animation frame `ctx.setTransform(dpr*zoom,0,0,dpr*zoom, pan.x*dpr*zoom, pan.y*dpr*zoom)`, draw grid, then `elements` sorted by `order` via `renderElement`, then a selection overlay (bounds + `resizeHandles`) for `selectedElementIds`. Keep wheel-zoom and space/hand pan. Create one `RoughCanvas` from the canvas.

- [ ] **Step 3: Verify visually** — `bun run dev`, add a couple elements from the console store (`useEditorStore.getState().addElement(makeElement("rectangle",{x:40,y:40,width:120,height:80},{styleMode:"sketch"}))`) and confirm a sketchy rect and a clean rect both render, pan/zoom work. Run `bun run check`.

- [ ] **Step 4: Commit** — `git commit -m "feat(draw): Canvas 2D renderer with clean + sketch + freehand"`

---

## Task 4: Tool controller — create elements per tool

**Files:** Create `src/tools/toolController.ts`; wire pointer handlers in `Canvas.tsx`.

**Interfaces:**
- Consumes: store, `makeElement` (T1).
- Produces: `function beginTool(tool, worldPt) / updateTool(worldPt) / endTool()` that create/extend the in-progress element and commit it via `addElement`.

- [ ] **Step 1** Implement drag-to-create for `rectangle`, `ellipse`, `line`, `arrow`, `sticky` (drag box); click-to-place + inline `<textarea>` overlay for `text`; `pen` accumulates `perfect-freehand` points on pointer-move; `eraser` removes elements under the cursor via `hitElement`. Snap to grid when `board.gridSnap`.
- [ ] **Step 2** Wire `Canvas.tsx` `onPointerDown/Move/Up` to call the controller when `activeTool !== "select" && !== "hand"`.
- [ ] **Step 3** Verify visually: each tool creates its shape; text editing commits on blur/Esc.
- [ ] **Step 4** Commit — `git commit -m "feat(draw): tool controller creates every shape type"`

---

## Task 5: Selection interactions

**Files:** Modify `Canvas.tsx`, add `src/tools/selection.ts`.

- [ ] **Step 1** Implement: click select (hit-test topmost), shift multi-select, marquee select (`hitInMarquee`), move (drag selected), resize via the 8 handles (update `data`), rotate via the rotate handle (update `transform`), delete (Del), copy/paste/duplicate (Ctrl+C/V/D, offset paste), z-order via `reorderElements`.
- [ ] **Step 2** Add a keyboard handler (mount-level) for the shortcuts in the spec.
- [ ] **Step 3** Verify visually: select, move, resize, rotate, delete, duplicate all work; undo/redo (Ctrl+Z/Y) reverts them.
- [ ] **Step 4** Commit — `git commit -m "feat(draw): select, move, resize, rotate, clipboard, shortcuts"`

---

## Task 6: Properties panel wired

**Files:** Rewrite `src/components/PropertiesPanel.tsx`.

- [ ] **Step 1** Bind controls to the selected element(s): stroke + fill color inputs, strokeWidth slider, strokeStyle segmented (solid/dashed/dotted), opacity slider, corner radius (rect), font family/size/align (text/sticky), and a **clean/sketch toggle** that sets `style.styleMode`. Each change calls `updateElement(id, { style: {...} })` for every selected id. Empty selection → show the board `defaultStyleMode` + grid-snap toggle.
- [ ] **Step 2** Verify visually: changing any control updates the canvas; multi-select applies to all.
- [ ] **Step 3** Commit — `git commit -m "feat(draw): properties panel edits full style incl. per-element sketch toggle"`

---

## Task 7: Layers panel wired

**Files:** Rewrite `src/components/LayerPanel.tsx`.

- [ ] **Step 1** List elements by `order` (top first) with type icon + name; click selects; up/down buttons reorder via `reorderElements`; hide (`style.opacity`→0 stored as `hidden` flag on `data`), lock (skip in hit-test), rename (`data.name`), delete.
- [ ] **Step 2** Make `hitElement`/selection skip `data.locked` and `data.hidden` elements.
- [ ] **Step 3** Verify visually. Commit — `git commit -m "feat(draw): layer panel reorder/hide/lock/rename/delete"`

---

## Task 8: Export PNG + SVG

**Files:** Create `src/export/exportPng.ts`, `src/export/exportSvg.ts`, `src/render/renderElementSvg.ts`, `src/render/renderElementSvg.test.ts`. Wire buttons into `TopBar.tsx`.

**Interfaces:** `renderElementSvg(el, mode): string` (SVG fragment); `exportPng(elements, opts): Blob`; `exportSvg(elements): string`.

- [ ] **Step 1: Failing test for SVG emit**

```ts
// src/render/renderElementSvg.test.ts
import { describe, it, expect } from "vitest";
import { renderElementSvg } from "./renderElementSvg";
import { makeElement } from "../stores/model";

it("emits an SVG rect for a clean rectangle", () => {
  const el = makeElement("rectangle", { x:0, y:0, width:10, height:10 }, { styleMode:"clean", stroke:"#f00" });
  const svg = renderElementSvg(el, "clean");
  expect(svg).toContain("<rect");
  expect(svg).toContain('stroke="#f00"');
});
```

- [ ] **Step 2: Run → FAIL.**
- [ ] **Step 3: Implement** `renderElementSvg` (clean = literal SVG elements; sketch = `roughjs`'s SVG generator `rough.svg().rectangle(...)` → serialize the node). `exportSvg` wraps all fragments in an `<svg>` sized to the union bounds. `exportPng` draws to an offscreen canvas at 2× and returns `toBlob`. `TopBar` buttons trigger a download.
- [ ] **Step 4: Run tests → PASS**; verify visually a board exports to a valid PNG + SVG.
- [ ] **Step 5: Commit** — `git commit -m "feat(draw): PNG + SVG export"`

---

## Task 9: Local persistence

**Files:** Create `src/persistence/localStore.ts`, `src/persistence/localStore.test.ts`; wire autosave + import/export into `TopBar.tsx`/`App.tsx`.

**Interfaces:** `serializeBoard(board, elements): string`; `deserializeBoard(json): {board,elements}`; `saveLocal()/loadLocal()`.

- [ ] **Step 1: Failing round-trip test**

```ts
// src/persistence/localStore.test.ts
import { describe, it, expect } from "vitest";
import { serializeBoard, deserializeBoard } from "./localStore";
import { makeElement } from "../stores/model";

it("round-trips a board with elements", () => {
  const els = [makeElement("rectangle", { x:1,y:2,width:3,height:4 })];
  const board = { id:"b1", name:"Test", description:"", width:1920, height:1080, background:"#0a0a0f", isPublic:false, defaultStyleMode:"clean" as const, gridSnap:true, elements:[] };
  const { board: b2, elements: e2 } = deserializeBoard(serializeBoard(board, els));
  expect(b2.name).toBe("Test");
  expect(e2[0].data.width).toBe(3);
  expect(e2[0].seed).toBe(els[0].seed);
});
```

- [ ] **Step 2: Run → FAIL.**
- [ ] **Step 3: Implement** serialize/deserialize (JSON with a `version` field), `saveLocal` (debounced write to `localStorage["nexus-draw:board"]`), `loadLocal`. Subscribe the store to autosave on element changes; load on mount; add New / Import(.json) / Export(.json) in `TopBar`.
- [ ] **Step 4: Run tests → PASS**; verify visually: reload keeps the board.
- [ ] **Step 5: Commit** — `git commit -m "feat(draw): localStorage autosave + board import/export"`

---

## Task 10: Toolbar cleanup, shortcuts overlay, polish

**Files:** Modify `Toolbar.tsx`, add `src/components/ShortcutsOverlay.tsx`, tidy `App.tsx`.

- [ ] **Step 1** Remove the fake `fill`/`zoom` tool buttons; use `lucide-react` icons instead of unicode; add a style-mode quick toggle + grid-snap toggle in the top bar; add `?` shortcuts overlay. Empty-canvas hint text.
- [ ] **Step 2** `bun run check` + full `npx vitest run` green.
- [ ] **Step 3** Commit — `git commit -m "feat(draw): toolbar polish, icons, shortcuts overlay"`

---

## Task 11: Build + deploy to draw.tnhc.dev

**Files:** none (deploy).

- [ ] **Step 1** `cd apps/Nexus-Draw/frontend && bun run build` → clean `dist/`.
- [ ] **Step 2** Deploy the new `dist/` to the existing `draw.tnhc.dev` site (id 5) via the Hosting API — for each file in `dist`: `POST /sites/5/files/upload-url` → PUT → `POST /sites/5/files`; then `POST /sites/5/deploy` (reuse the `fh_` token minted earlier, or mint a fresh one per the site-deploy-flow memory). Set correct content types (text/html, text/javascript, text/css, plus any fonts/images).
- [ ] **Step 3** Verify at the edge: `curl -sI https://draw.tnhc.dev/` → 200; load in a browser (or screenshot) and confirm shapes/pen/text/export all work. Commit the built `dist` if the repo tracks it (it does): `git commit -m "chore(draw): rebuild and deploy editor to draw.tnhc.dev"`.

---

## Self-Review (plan author)

- **Spec coverage:** rendering→T3; model/style→T1; geometry/hit→T2; tools→T4; selection/shortcuts→T5; properties→T6; layers→T7; export→T8; persistence→T9; polish→T10; ship→T11. Every spec section maps to a task.
- **Placeholders:** pure-logic tasks (1,2,8,9) carry real test + impl code; imperative rendering/UI tasks (3–7,10) specify exact files, functions, store calls, and a visual-verification step — the honest structure for canvas code that isn't unit-testable.
- **Type consistency:** `ElementData`/`ElementStyle`/`StyleMode` from `model.ts` (T1) are used verbatim in T2–T9; `resolveStyleMode`, `elementBounds`, `resizeHandles`, `arrowHead`, `renderElement`, `renderElementSvg` names match across producer and consumer tasks.
- **Dependency order:** T1→T2→T3 are strict; T4–T10 depend on T1–T3; T11 last. Fits subagent-per-task with review between.
