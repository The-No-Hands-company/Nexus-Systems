# Nexus-Draw Phase 2 Implementation Plan — Connectors, Images, Templates

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add three phase-2 features to the Nexus-Draw frontend: glued auto-routing connectors between shapes, an image tool with drag-and-drop + paste, and a data-defined template library with a sidebar panel.

**Architecture:** Connectors are a new element type (`"connector"`) whose polyline is computed at render time by a pure `routeConnector(el, elements)` so glued connectors follow shapes and collab peers automatically. Images are a `"image"` element with a `src` data URL (downscaled on import) rendered via a cached `HTMLImageElement` map. Templates are data-defined `build(): ElementData[]` functions inserted at the viewport center as one undo step.

**Tech Stack:** React 19, Vite, zustand 5, vitest, tailwind, roughjs, perfect-freehand. All work in `apps/Nexus-Draw/frontend`.

## Global Constraints

- Run the gate from `apps/Nexus-Draw/frontend`: `bun run check && bun test` (tsc `--noEmit` + vitest). All tasks end with this passing.
- Strict TypeScript (Bun). No new runtime dependencies.
- Element data must stay plain JSON — `collab/yElements.ts` (`toStored`/`writeElements`) and `localStore` serialize it as-is; no schema changes there.
- Renderer/pointer paths are visual (verified by build + run), per the established testing philosophy; pure geometry/model/SVG paths carry unit tests.
- Follow existing code style: no comments unless explaining non-obvious intent (existing files use explanatory comments — match that tone, but don't add gratuitous ones).
- Branch note: this plan's commits belong on `main`. The working tree is currently on the other session's branch — check out `main` (or a worktree) before executing.

---

## File Structure

- `src/stores/model.ts` — add `"connector"` to `ElementType`.
- `src/render/geometry.ts` — add `boundsCenter`, `shapeEdgePoint`, `orthogonalBetween`, `ConnectorData`, `routeConnector`; extend `elementBounds` with a connector branch + optional `elements` param.
- `src/render/hitTest.ts` — add connector branch to `hitElement` + optional `elements` param.
- `src/tools/selection.ts` — connector branch in `translateElement`; thread `elements` through `elementWorldBounds`/`elementInMarquee`.
- `src/tools/toolController.ts` — `DraftConnector`, connector tool state machine, `connectorData`, `DEFAULT_ROUTING`, `commitConnectorDraft`, `draftToElement` connector branch.
- `src/render/renderElement.ts` — connector + real-image render branches; optional `elements` param on `renderElement`.
- `src/utils/export.ts` — connector SVG branch; `svgElement`/`downloadPNG` thread `elements`.
- `src/utils/imageImport.ts` — NEW: `computeDownscale`, `pickEncoding`, `loadImageToDataUrl`.
- `src/templates/index.ts` — NEW: `Template` interface + `templates` array (Flowchart, Mind Map, Org Chart, Grid, Kanban).
- `src/templates/insertTemplate.ts` — NEW: `insertTemplate` + `templateBounds` + `offsetForTemplate`.
- `src/components/TemplatesPanel.tsx` — NEW: sidebar panel listing templates.
- `src/components/Canvas/Canvas.tsx` — connector tool wiring (double-click/Enter/Escape), image file picker + DnD + paste, thread `elements` through render/hit/overlay calls, keyboard `C`/`I`.
- `src/components/Toolbar.tsx` — add `connector` (C) and `image` (I) tools.
- `src/components/HelpOverlay.tsx` — add rows for C and I.
- `src/components/PropertiesPanel.tsx` — connector routing toggle.
- `src/App.tsx` + `src/stores/useEditorStore.ts` — sidebar union gains `"templates"` + TemplatesPanel wiring + bottom-bar button.
- Tests: `src/render/geometry.test.ts` (NEW), `src/render/hitTest.test.ts`, `src/tools/selection.test.ts`, `src/tools/toolController.test.ts`, `src/utils/export.test.ts`, `src/utils/imageImport.test.ts` (NEW), `src/templates/templates.test.ts` (NEW), `src/utils/persistence.test.ts`.

---

## Phase A — Connectors

### Task 1: Add `"connector"` element type

**Files:**
- Modify: `src/stores/model.ts:2`
- Test: `src/stores/model.test.ts`

**Interfaces:**
- Produces: `ElementType` now includes `"connector"`.

- [ ] **Step 1: Write the failing test**

Append to `src/stores/model.test.ts`:

```ts
describe("makeElement — connector", () => {
  it("creates a connector element type", () => {
    const el = makeElement("connector", { startId: "a", endId: "b", routing: "elbow" });
    expect(el.elementType).toBe("connector");
    expect(el.data).toMatchObject({ startId: "a", endId: "b", routing: "elbow" });
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test src/stores/model.test.ts`
Expected: FAIL — `elementType` is `"rect...` (connector not in the union, so `makeElement`'s return type won't matter but the value will be wrong — the type literal rejects `"connector"`).

- [ ] **Step 3: Add the type**

In `src/stores/model.ts:2`, change:

```ts
export type ElementType = "rectangle"|"ellipse"|"line"|"arrow"|"freehand"|"text"|"sticky"|"image";
```

to:

```ts
export type ElementType = "rectangle"|"ellipse"|"line"|"arrow"|"freehand"|"text"|"sticky"|"image"|"connector";
```

- [ ] **Step 4: Run test to verify it passes**

Run: `bun test src/stores/model.test.ts`
Expected: PASS

- [ ] **Step 5: Gate + commit**

```bash
bun run check && bun test
git add src/stores/model.ts src/stores/model.test.ts
git commit -m "feat: add connector element type"
```

---

### Task 2: Connector routing geometry (pure)

**Files:**
- Modify: `src/render/geometry.ts`
- Create: `src/render/geometry.test.ts`

**Interfaces:**
- Consumes: `ElementType` includes `"connector"` (Task 1).
- Produces (later tasks rely on these exact signatures):
  - `export interface ConnectorData { startId?: string; endId?: string; startPoint?: Point; endPoint?: Point; waypoints?: Point[]; routing: "elbow" | "straight" }`
  - `export function boundsCenter(b: Bounds): Point`
  - `export function shapeEdgePoint(b: Bounds, target: Point): Point`
  - `export function orthogonalBetween(a: Point, b: Point): Point[]`
  - `export function routeConnector(el: ElementData, elements: ElementData[]): Point[]`
  - `elementBounds(el: ElementData, elements?: ElementData[]): Bounds` (extended)

- [ ] **Step 1: Write the failing tests**

Create `src/render/geometry.test.ts`:

```ts
import { describe, it, expect } from "vitest";
import { makeElement, type ElementData } from "../stores/model";
import { elementBounds, shapeEdgePoint, orthogonalBetween, routeConnector } from "./geometry";

describe("shapeEdgePoint", () => {
  it("returns the right-edge point when the target is to the right", () => {
    const b = { x: 0, y: 0, width: 100, height: 50 };
    const p = shapeEdgePoint(b, { x: 200, y: 25 });
    expect(p.x).toBeCloseTo(100, 8);
    expect(p.y).toBeCloseTo(25, 8);
  });
  it("returns the bottom-edge point when the target is below", () => {
    const b = { x: 0, y: 0, width: 100, height: 50 };
    const p = shapeEdgePoint(b, { x: 50, y: 200 });
    expect(p.x).toBeCloseTo(50, 8);
    expect(p.y).toBeCloseTo(50, 8);
  });
  it("falls back to the center when the target is the center", () => {
    const b = { x: 10, y: 20, width: 100, height: 50 };
    expect(shapeEdgePoint(b, { x: 60, y: 45 })).toEqual({ x: 60, y: 45 });
  });
});

describe("orthogonalBetween", () => {
  it("routes horizontally-primary with a mid-x S", () => {
    const pts = orthogonalBetween({ x: 0, y: 0 }, { x: 100, y: 40 });
    expect(pts).toEqual([
      { x: 0, y: 0 },
      { x: 50, y: 0 },
      { x: 50, y: 40 },
      { x: 100, y: 40 },
    ]);
  });
  it("routes vertically-primary with a mid-y S", () => {
    const pts = orthogonalBetween({ x: 0, y: 0 }, { x: 40, y: 100 });
    expect(pts).toEqual([
      { x: 0, y: 0 },
      { x: 0, y: 50 },
      { x: 40, y: 50 },
      { x: 40, y: 100 },
    ]);
  });
  it("keeps a straight line when already axis-aligned", () => {
    expect(orthogonalBetween({ x: 0, y: 0 }, { x: 0, y: 60 })).toEqual([{ x: 0, y: 0 }, { x: 0, y: 60 }]);
  });
});

describe("routeConnector", () => {
  const boxA = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
  const boxB = makeElement("rectangle", { x: 300, y: 0, width: 100, height: 50 });
  const elements = [boxA, boxB];

  it("routes a free straight connector between its points", () => {
    const el = makeElement("connector", { startPoint: { x: 0, y: 0 }, endPoint: { x: 100, y: 50 }, routing: "straight" });
    const pts = routeConnector(el, elements);
    expect(pts).toEqual([{ x: 0, y: 0 }, { x: 100, y: 50 }]);
  });

  it("routes a free elbow connector with orthogonal bends", () => {
    const el = makeElement("connector", { startPoint: { x: 0, y: 0 }, endPoint: { x: 100, y: 40 }, routing: "elbow" });
    const pts = routeConnector(el, elements);
    expect(pts[0]).toEqual({ x: 0, y: 0 });
    expect(pts[pts.length - 1]).toEqual({ x: 100, y: 40 });
    // every segment is axis-aligned
    for (let i = 0; i < pts.length - 1; i++) {
      const dx = Math.abs(pts[i + 1].x - pts[i].x);
      const dy = Math.abs(pts[i + 1].y - pts[i].y);
      expect(dx === 0 || dy === 0).toBe(true);
    }
  });

  it("glues to shapes and re-routes when a shape moves", () => {
    const el = makeElement("connector", { startId: boxA.id, endId: boxB.id, routing: "elbow" });
    const before = routeConnector(el, elements);
    expect(before[0].x).toBeCloseTo(100, 8); // exits A's right edge
    expect(before[before.length - 1].x).toBeCloseTo(300, 8); // enters B's left edge

    const moved = [...elements.map((e) => (e.id === boxB.id ? { ...e, data: { ...e.data, x: 500 } } : e))];
    const after = routeConnector(el, moved);
    expect(after[after.length - 1].x).toBeCloseTo(500, 8);
  });

  it("falls back to stored points when a glued id is missing", () => {
    const el = makeElement("connector", { startId: "ghost", endId: boxB.id, startPoint: { x: 0, y: 0 }, routing: "straight" });
    const pts = routeConnector(el, elements);
    expect(pts[0]).toEqual({ x: 0, y: 0 });
    expect(pts[pts.length - 1].x).toBeCloseTo(300, 8);
  });

  it("splices waypoints into the path", () => {
    const el = makeElement("connector", {
      startPoint: { x: 0, y: 0 },
      endPoint: { x: 200, y: 0 },
      waypoints: [{ x: 100, y: 80 }],
      routing: "elbow",
    });
    const pts = routeConnector(el, elements);
    // the route must pass through (100, 80)
    expect(pts.some((p) => p.x === 100 && p.y === 80)).toBe(true);
  });
});

describe("elementBounds — connector", () => {
  it("returns the AABB of the routed path", () => {
    const boxA = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
    const boxB = makeElement("rectangle", { x: 300, y: 0, width: 100, height: 50 });
    const el = makeElement("connector", { startId: boxA.id, endId: boxB.id, routing: "straight" });
    const b = elementBounds(el, [boxA, boxB]);
    expect(b.x).toBeCloseTo(100, 8);
    expect(b.y).toBeCloseTo(25, 8);
    expect(b.width).toBeGreaterThan(100);
    expect(b.height).toBeLessThanOrEqual(1e-6);
  });
  it("falls back to zero bounds without elements", () => {
    const el = makeElement("connector", { startId: "ghost", endId: "other", routing: "straight" });
    expect(elementBounds(el)).toEqual({ x: 0, y: 0, width: 0, height: 0 });
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `bun test src/render/geometry.test.ts`
Expected: FAIL — `shapeEdgePoint`/`orthogonalBetween`/`routeConnector` don't exist.

- [ ] **Step 3: Implement in `src/render/geometry.ts`**

Add after the existing `distToSegment`/`elementBounds` helpers (imports already include `ElementData`):

```ts
/** Center of a bounds box. */
export function boundsCenter(b: Bounds): Point {
  return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
}

/**
 * The point on the AABB boundary of `b` where a ray from its center toward
 * `target` exits. Used to anchor a connector to the edge of a glued shape.
 * Degenerates to the center when target coincides with it.
 */
export function shapeEdgePoint(b: Bounds, target: Point): Point {
  const cx = b.x + b.width / 2;
  const cy = b.y + b.height / 2;
  const dx = target.x - cx;
  const dy = target.y - cy;
  if (dx === 0 && dy === 0) return { x: cx, y: cy };
  const halfW = b.width / 2;
  const halfH = b.height / 2;
  let t = Infinity;
  if (dx !== 0) t = Math.min(t, Math.abs(halfW / dx));
  if (dy !== 0) t = Math.min(t, Math.abs(halfH / dy));
  return { x: cx + dx * t, y: cy + dy * t };
}

/**
 * Orthogonal (elbow) route between two points: a 3-segment S via the midpoint
 * of the dominant axis. Returns [a, mid1, mid2, b]; axis-aligned inputs pass
 * straight through as [a, b]. Draw.io-lite — no obstacle avoidance.
 */
export function orthogonalBetween(a: Point, b: Point): Point[] {
  if (a.x === b.x || a.y === b.y) return [a, b];
  if (Math.abs(b.x - a.x) >= Math.abs(b.y - a.y)) {
    const midX = (a.x + b.x) / 2;
    return [a, { x: midX, y: a.y }, { x: midX, y: b.y }, b];
  }
  const midY = (a.y + b.y) / 2;
  return [a, { x: a.x, y: midY }, { x: b.x, y: midY }, b];
}

export interface ConnectorData {
  startId?: string;
  endId?: string;
  startPoint?: Point;
  endPoint?: Point;
  waypoints?: Point[];
  routing: "elbow" | "straight";
}

function dedupePoints(pts: Point[]): Point[] {
  return pts.filter((p, i) => i === 0 || p.x !== pts[i - 1].x || p.y !== pts[i - 1].y);
}

/**
 * The connector's full polyline in world space, computed fresh from the live
 * element list so glued connectors follow moved/resized/rotated shapes (and
 * collab peer edits) automatically. Glued endpoints anchor to the shape edge
 * nearest the other endpoint; free connectors route between stored points.
 * Missing glued ids fall back to stored points (or origin) so rendering never
 * throws on stale refs.
 */
export function routeConnector(el: ElementData, elements: ElementData[]): Point[] {
  const d = el.data as ConnectorData;
  const startShape = d.startId ? elements.find((e) => e.id === d.startId) : undefined;
  const endShape = d.endId ? elements.find((e) => e.id === d.endId) : undefined;

  const endTarget: Point = endShape
    ? boundsCenter(elementBounds(endShape))
    : d.endPoint ?? { x: 0, y: 0 };
  const startTarget: Point = startShape
    ? boundsCenter(elementBounds(startShape))
    : d.startPoint ?? { x: 0, y: 0 };

  const start: Point = startShape
    ? shapeEdgePoint(elementBounds(startShape), endTarget)
    : d.startPoint ?? { x: 0, y: 0 };
  const end: Point = endShape
    ? shapeEdgePoint(elementBounds(endShape), startTarget)
    : d.endPoint ?? start;

  const anchors = [start, ...(d.waypoints ?? []), end];

  if (d.routing === "straight") {
    return dedupePoints(anchors);
  }
  const pts: Point[] = [anchors[0]];
  for (let i = 0; i < anchors.length - 1; i++) {
    pts.push(...orthogonalBetween(anchors[i], anchors[i + 1]).slice(1));
  }
  return dedupePoints(pts);
}
```

Then extend `elementBounds` — add a connector branch before the freehand branch (and give it an optional `elements` param):

```ts
export function elementBounds(el: ElementData, elements?: ElementData[]): Bounds {
  const d = el.data as Record<string, any>;

  if (el.elementType === "connector") {
    const pts = routeConnector(el, elements ?? []);
    if (pts.length === 0) return { x: 0, y: 0, width: 0, height: 0 };
    const xs = pts.map((p) => p.x);
    const ys = pts.map((p) => p.y);
    const minX = Math.min(...xs);
    const minY = Math.min(...ys);
    return { x: minX, y: minY, width: Math.max(...xs) - minX, height: Math.max(...ys) - minY };
  }
  // ...existing branches unchanged...
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `bun test src/render/geometry.test.ts`
Expected: PASS

- [ ] **Step 5: Gate + commit**

```bash
bun run check && bun test
git add src/render/geometry.ts src/render/geometry.test.ts
git commit -m "feat: connector routing geometry"
```

---

### Task 3: Connector hit-testing

**Files:**
- Modify: `src/render/hitTest.ts:19`
- Test: `src/render/hitTest.test.ts`

**Interfaces:**
- Consumes: `routeConnector`, `ConnectorData` (Task 2).
- Produces: `hitElement(el: ElementData, p: Point, tol: number, elements?: ElementData[]): boolean` (optional 4th param).

- [ ] **Step 1: Write the failing test**

Append to `src/render/hitTest.test.ts`:

```ts
describe("hitElement — connector", () => {
  const a = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
  const b = makeElement("rectangle", { x: 300, y: 0, width: 100, height: 50 });
  const conn = makeElement("connector", { startId: a.id, endId: b.id, routing: "straight" });
  const elements = [a, b];

  it("hits a point on the line between the two shapes", () => {
    expect(hitElement(conn, { x: 200, y: 25 }, 4, elements)).toBe(true);
  });
  it("misses a point far from the line", () => {
    expect(hitElement(conn, { x: 200, y: 100 }, 4, elements)).toBe(false);
  });
  it("tolerates points within stroke+tolerance of the line", () => {
    expect(hitElement(conn, { x: 200, y: 30 }, 4, elements)).toBe(true);
  });
  it("never hits when elements are missing (falls back to origin points)", () => {
    expect(hitElement(conn, { x: 200, y: 25 }, 4)).toBe(false);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test src/render/hitTest.test.ts`
Expected: FAIL — connectors hit via the box fallback (origin, 0×0) so all points miss.

- [ ] **Step 3: Implement in `src/render/hitTest.ts`**

Update the import line (`import { distToSegment, elementBounds, ... } from "./geometry"`) to also import `routeConnector`. Then change the signature and add the branch:

```ts
export function hitElement(el: ElementData, p: Point, tol: number, elements?: ElementData[]): boolean {
  const d = el.data as Record<string, any>;
  if (d.locked || d.hidden) return false;
  const strokeWidth = el.style?.strokeWidth ?? 0;

  if (el.elementType === "connector") {
    const pts = routeConnector(el, elements ?? []);
    if (pts.length < 2) return false;
    let min = Infinity;
    for (let i = 0; i < pts.length - 1; i++) {
      min = Math.min(min, distToSegment(p, pts[i], pts[i + 1]));
    }
    return min <= tol + strokeWidth;
  }

  // ...existing branches unchanged...
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `bun test src/render/hitTest.test.ts`
Expected: PASS (existing tests still pass — `elements` is optional).

- [ ] **Step 5: Gate + commit**

```bash
bun run check && bun test
git add src/render/hitTest.ts src/render/hitTest.test.ts
git commit -m "feat: connector hit-testing"
```

---

### Task 4: Connector selection/transform behavior

**Files:**
- Modify: `src/tools/selection.ts`
- Test: `src/tools/selection.test.ts`

**Interfaces:**
- Consumes: `ConnectorData` (Task 2).
- Produces:
  - `translateElement(el, dx, dy)` — connector branch: glued connectors are returned unchanged (they follow shapes); free connectors shift `startPoint`/`endPoint`/`waypoints`.
  - `elementWorldBounds(el, elements?: ElementData[])` — threads `elements` to `elementBounds`.
  - `elementInMarquee(el, rect, elements?: ElementData[])` — threads `elements`.

- [ ] **Step 1: Write the failing tests**

Append to `src/tools/selection.test.ts`:

```ts
describe("translateElement — connector", () => {
  it("shifts a free connector's points", () => {
    const el = makeElement("connector", {
      startPoint: { x: 0, y: 0 },
      endPoint: { x: 100, y: 0 },
      waypoints: [{ x: 50, y: 40 }],
      routing: "elbow",
    });
    const moved = translateElement(el, 10, 5);
    expect(moved.data.startPoint).toEqual({ x: 10, y: 5 });
    expect(moved.data.endPoint).toEqual({ x: 110, y: 5 });
    expect(moved.data.waypoints).toEqual([{ x: 60, y: 45 }]);
  });
  it("does not shift a glued connector (it follows its shapes)", () => {
    const el = makeElement("connector", { startId: "a", endId: "b", routing: "elbow" });
    expect(translateElement(el, 10, 5)).toBe(el);
  });
});

describe("resizeElement — connector", () => {
  it("returns the element unchanged (no resize handles)", () => {
    const el = makeElement("connector", { startPoint: { x: 0, y: 0 }, endPoint: { x: 100, y: 0 }, routing: "elbow" });
    expect(resizeElement(el, "se", 50, 50)).toBe(el);
  });
});

describe("elementWorldBounds — connector", () => {
  it("uses the routed path from the element list", () => {
    const a = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
    const b = makeElement("rectangle", { x: 300, y: 0, width: 100, height: 50 });
    const el = makeElement("connector", { startId: a.id, endId: b.id, routing: "straight" });
    const wb = elementWorldBounds(el, [a, b]);
    expect(wb.x).toBeCloseTo(100, 8);
    expect(wb.y).toBeCloseTo(25, 8);
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `bun test src/tools/selection.test.ts`
Expected: FAIL — connector falls through to the box branch (shifts `x`/`y`, ignores points).

- [ ] **Step 3: Implement in `src/tools/selection.ts`**

In `translateElement`, add a connector branch before the line/arrow branch:

```ts
if (el.elementType === "connector") {
  const c = el.data as any;
  if (c.startId || c.endId) return el; // glued connectors follow their shapes
  return {
    ...el,
    data: {
      ...c,
      startPoint: c.startPoint ? { x: c.startPoint.x + dx, y: c.startPoint.y + dy } : undefined,
      endPoint: c.endPoint ? { x: c.endPoint.x + dx, y: c.endPoint.y + dy } : undefined,
      waypoints: (c.waypoints ?? []).map((w: Point) => ({ x: w.x + dx, y: w.y + dy })),
    },
  };
}
```

`resizeElement` already falls through to `return el` for anything that isn't a box or line/arrow — no change needed.

Update `elementWorldBounds` and `elementInMarquee` signatures to thread `elements`:

```ts
export function elementWorldBounds(el: ElementData, elements?: ElementData[]): Bounds {
  const b = elementBounds(el, elements);
  // ...rest unchanged...
}

export function elementInMarquee(el: ElementData, rect: Bounds, elements?: ElementData[]): boolean {
  const b = elementWorldBounds(el, elements);
  // ...rest unchanged...
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `bun test src/tools/selection.test.ts`
Expected: PASS

- [ ] **Step 5: Gate + commit**

```bash
bun run check && bun test
git add src/tools/selection.ts src/tools/selection.test.ts
git commit -m "feat: connector selection and transform behavior"
```

---

### Task 5: Connector rendering (canvas + SVG export)

**Files:**
- Modify: `src/render/renderElement.ts`
- Modify: `src/utils/export.ts`
- Test: `src/utils/export.test.ts`

**Interfaces:**
- Consumes: `routeConnector` (Task 2), `arrowHead` (existing).
- Produces:
  - `renderElement(ctx, rc, el, mode, elements?: ElementData[])` — connector branch.
  - `svgElement(el, mode, elements?: ElementData[])` — connector branch.
  - `buildSvgDataUrl(board, elements)` and `downloadPNG(board, elements)` pass `elements` into the render/svg calls.

- [ ] **Step 1: Write the failing test**

Append to `src/utils/export.test.ts`:

```ts
test("exports a glued connector as a polyline plus arrowhead", () => {
  const a = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
  const b = makeElement("rectangle", { x: 300, y: 0, width: 100, height: 50 });
  const conn = makeElement("connector", { startId: a.id, endId: b.id, routing: "straight" });
  const svg = decodeURIComponent(buildSvgDataUrl(board, [a, b, conn]).split(",")[1]);
  expect(svg).toContain("<polyline");
  expect(svg).toContain('<polygon');
});

test("exports a free elbow connector with orthogonal segments", () => {
  const conn = makeElement("connector", { startPoint: { x: 0, y: 0 }, endPoint: { x: 100, y: 40 }, routing: "elbow" });
  const svg = decodeURIComponent(buildSvgDataUrl(board, [conn]).split(",")[1]);
  const m = svg.match(/points="([^"]+)"/);
  expect(m).not.toBeNull();
  const pts = m![1].split(" ").map((p) => p.split(",").map(Number));
  for (let i = 0; i < pts.length - 1; i++) {
    const dx = Math.abs(pts[i + 1][0] - pts[i][0]);
    const dy = Math.abs(pts[i + 1][1] - pts[i][1]);
    expect(dx === 0 || dy === 0).toBe(true);
  }
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test src/utils/export.test.ts`
Expected: FAIL — no connector case in `svgElement`, so no polyline emitted.

- [ ] **Step 3: Implement in `src/render/renderElement.ts`**

Update the import to include `routeConnector` from `./geometry`. Change `renderElement` signature to accept an optional `elements` list and add the connector case plus a render helper:

```ts
function renderConnector(
  ctx: CanvasRenderingContext2D,
  rc: RoughCanvas,
  el: ElementData,
  mode: StyleMode,
  elements?: ElementData[]
): void {
  const pts = routeConnector(el, elements ?? []);
  if (pts.length < 2) return;
  const style = el.style;
  const headSize = Math.max(12, style.strokeWidth * 5);
  const last = pts[pts.length - 1];
  const prev = pts[pts.length - 2];
  const { p1, p2 } = arrowHead(prev.x, prev.y, last.x, last.y, headSize);

  if (mode === "sketch") {
    const opts = roughOptions(el, { fill: undefined });
    for (let i = 0; i < pts.length - 1; i++) {
      rc.line(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, opts);
    }
    rc.polygon([[last.x, last.y], p1, p2], { ...opts, fill: style.stroke, fillStyle: "solid" });
    return;
  }

  ctx.save();
  applyStrokeStyle(ctx, style);
  ctx.beginPath();
  ctx.moveTo(pts[0].x, pts[0].y);
  for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
  ctx.stroke();

  ctx.setLineDash([]);
  ctx.beginPath();
  ctx.moveTo(last.x, last.y);
  ctx.lineTo(p1[0], p1[1]);
  ctx.lineTo(p2[0], p2[1]);
  ctx.closePath();
  ctx.fillStyle = style.stroke;
  ctx.fill();
  ctx.restore();
}

export function renderElement(
  ctx: CanvasRenderingContext2D,
  rc: RoughCanvas,
  el: ElementData,
  mode: StyleMode,
  elements?: ElementData[]
): void {
  ctx.save();
  ctx.globalAlpha = el.style.opacity;
  switch (el.elementType) {
    // ...existing cases unchanged...
    case "connector":
      renderConnector(ctx, rc, el, mode, elements);
      break;
    // ...default unchanged...
  }
  ctx.restore();
}
```

- [ ] **Step 4: Implement in `src/utils/export.ts`**

Import `routeConnector` from `../render/geometry`. Add a connector case to `svgElement` and thread `elements`:

```ts
function svgElement(el: ElementData, mode: StyleMode, elements?: ElementData[]): string {
  // ...existing code, then inside the switch:
  case "connector": {
    const pts = routeConnector(el, elements ?? []);
    if (pts.length >= 2) {
      const poly = pts.map((p) => `${p.x},${p.y}`).join(" ");
      body.push(`<polyline points="${poly}" ${stroke}${opacity}/>`);
      const last = pts[pts.length - 1];
      const prev = pts[pts.length - 2];
      const { p1, p2 } = arrowHead(prev.x, prev.y, last.x, last.y, 12 + s.strokeWidth * 2);
      body.push(`<polygon points="${last.x},${last.y} ${p1[0]},${p1[1]} ${p2[0]},${p2[1]}" ${common} fill="${s.stroke}"/>`);
    }
    break;
  }
}
```

In `buildSvgDataUrl`, update the map call:

```ts
const inner = sorted.filter((el) => !el.data.hidden).map((el) => svgElement(el, resolveStyleMode(el, mode), elements)).join("");
```

In `downloadPNG`, pass `elements` to `renderElement`:

```ts
for (const el of sorted) renderElement(ctx, rc, el, resolveStyleMode(el, mode), elements);
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `bun test src/utils/export.test.ts`
Expected: PASS (existing tests pass — `elements` is optional).

- [ ] **Step 6: Gate + commit**

```bash
bun run check && bun test
git add src/render/renderElement.ts src/utils/export.ts src/utils/export.test.ts
git commit -m "feat: render connectors on canvas and in SVG export"
```

---

### Task 6: Connector tool state machine

**Files:**
- Modify: `src/tools/toolController.ts`
- Test: `src/tools/toolController.test.ts`

**Interfaces:**
- Consumes: `routeConnector` used via `hitElement` (Task 3), `makeElement` (existing).
- Produces:
  - `export const DEFAULT_ROUTING: "elbow" | "straight"` (default `"elbow"`).
  - `export interface DraftConnector { kind: "connector"; startId?: string; startPoint?: Point; beginPoint: Point; waypoints: Point[]; endPoint: Point; routing: "elbow" | "straight"; seed: number }`
  - `Draft` union gains `DraftConnector`.
  - `ToolController.commitConnectorDraft(): void` — commits the pending waypoint-mode connector as a free connector.
  - `ToolController.cancel()` also clears the pending connector.
  - `draftToElement(draft, boardDefault)` handles `kind: "connector"`.

Behavior contract (matches the spec):
- **Drag A→B:** `beginTool("connector", pt)` on a shape sets `startId`; drag updates `endPoint`; `endTool()` on a shape sets `endId`, on empty canvas sets free `endPoint`.
- **Waypoints:** a click (no drag) keeps the draft pending; the next `beginTool` appends a waypoint at the previous `endPoint` and extends; clicking a shape glues `endId` and commits; `commitConnectorDraft()` (double-click/Enter) commits free; Escape cancels.

- [ ] **Step 1: Write the failing tests**

Append to `src/tools/toolController.test.ts`:

```ts
import { DEFAULT_ROUTING, type DraftConnector } from "./toolController";

describe("ToolController — connector tool", () => {
  const setup = () => {
    useEditorStore.setState({ elements: [], selectedElementIds: new Set(), undoStack: [], redoStack: [] });
    const store = useEditorStore.getState();
    const rect = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
    rect.order = 0;
    store.setElementsLive([rect]);
    return rect;
  };

  it("beginTool on a shape sets startId", () => {
    const rect = setup();
    const controller = new ToolController();
    controller.beginTool("connector", { x: 50, y: 25 }, false);
    const draft = controller.draft as DraftConnector;
    expect(draft.kind).toBe("connector");
    expect(draft.startId).toBe(rect.id);
    expect(draft.waypoints).toEqual([]);
    expect(draft.routing).toBe(DEFAULT_ROUTING);
  });

  it("beginTool on empty canvas sets startPoint", () => {
    setup();
    const controller = new ToolController();
    controller.beginTool("connector", { x: 500, y: 500 }, false);
    const draft = controller.draft as DraftConnector;
    expect(draft.startId).toBeUndefined();
    expect(draft.startPoint).toEqual({ x: 500, y: 500 });
  });

  it("drag then release on a shape commits a glued connector", () => {
    const rect = setup();
    const target = makeElement("rectangle", { x: 300, y: 0, width: 100, height: 50 });
    useEditorStore.getState().setElementsLive([rect, target]);
    const controller = new ToolController();
    controller.beginTool("connector", { x: 50, y: 25 }, false);
    controller.updateTool({ x: 320, y: 25 }, false);
    controller.endTool();
    const els = useEditorStore.getState().elements;
    expect(els).toHaveLength(2);
    const conn = els.find((e) => e.elementType === "connector")!;
    expect(conn).toBeDefined();
    expect(conn.data.startId).toBe(rect.id);
    expect(conn.data.endId).toBe(target.id);
  });

  it("drag then release on empty canvas commits a free connector", () => {
    setup();
    const controller = new ToolController();
    // Start away from the setup rect (0,0..100,50) so the start is free-floating.
    controller.beginTool("connector", { x: -100, y: -100 }, false);
    controller.updateTool({ x: 320, y: 25 }, false);
    controller.endTool();
    const els = useEditorStore.getState().elements;
    const conn = els.find((e) => e.elementType === "connector")!;
    expect(conn.data.startId).toBeUndefined();
    expect(conn.data.endId).toBeUndefined();
    expect(conn.data.startPoint).toEqual({ x: -100, y: -100 });
    expect(conn.data.endPoint).toEqual({ x: 320, y: 25 });
  });

  it("click keeps the draft pending; second click appends a waypoint; Enter commits", () => {
    setup();
    const controller = new ToolController();
    // Use coordinates far from the setup rect (which occupies 0,0..100,50) so the
    // draft stays free-floating (startId undefined) and we exercise waypoints.
    controller.beginTool("connector", { x: -200, y: -200 }, false);
    controller.endTool(); // click 1 — no drag, stays pending
    expect(useEditorStore.getState().elements.filter((e) => e.elementType === "connector")).toHaveLength(0);

    controller.beginTool("connector", { x: -100, y: -200 }, false);
    controller.endTool(); // click 2 — previous endPoint becomes a waypoint
    controller.commitConnectorDraft(); // Enter / double-click

    const els = useEditorStore.getState().elements;
    const conn = els.find((e) => e.elementType === "connector")!;
    expect(conn).toBeDefined();
    expect(conn.data.waypoints).toHaveLength(1);
    expect(conn.data.waypoints[0]).toEqual({ x: -200, y: -200 });
    expect(conn.data.endPoint).toEqual({ x: -100, y: -200 });
    expect(conn.data.startId).toBeUndefined();
  });

  it("clicking a shape while pending glues the end and commits", () => {
    const rect = setup();
    const target = makeElement("rectangle", { x: 300, y: 0, width: 100, height: 50 });
    useEditorStore.getState().setElementsLive([rect, target]);
    const controller = new ToolController();
    controller.beginTool("connector", { x: 0, y: 0 }, false);
    controller.endTool(); // pending
    controller.beginTool("connector", { x: 320, y: 25 }, false);
    controller.endTool(); // this click lands on the target shape
    const conn = useEditorStore.getState().elements.find((e) => e.elementType === "connector")!;
    expect(conn.data.startId).toBe(rect.id);
    expect(conn.data.endId).toBe(target.id);
  });

  it("Escape cancels the pending connector", () => {
    setup();
    const controller = new ToolController();
    controller.beginTool("connector", { x: 0, y: 0 }, false);
    controller.endTool();
    expect(controller.draft).not.toBeNull();
    controller.cancel();
    expect(controller.draft).toBeNull();
    expect(useEditorStore.getState().elements.filter((e) => e.elementType === "connector")).toHaveLength(0);
  });
});

describe("draftToElement — connector", () => {
  it("builds a connector element from the draft", () => {
    const draft: DraftConnector = {
      kind: "connector",
      startId: undefined,
      startPoint: { x: 0, y: 0 },
      beginPoint: { x: 0, y: 0 },
      waypoints: [],
      endPoint: { x: 100, y: 0 },
      routing: "elbow",
      seed: 7,
    };
    const el = draftToElement(draft, "clean");
    expect(el.elementType).toBe("connector");
    expect(el.data.startPoint).toEqual({ x: 0, y: 0 });
    expect(el.data.endPoint).toEqual({ x: 100, y: 0 });
    expect(el.data.routing).toBe("elbow");
  });
});
```

(Add `makeElement` to the existing import from `../stores/model` if not already there.)

- [ ] **Step 2: Run tests to verify they fail**

Run: `bun test src/tools/toolController.test.ts`
Expected: FAIL — `DraftConnector`/`DEFAULT_ROUTING`/`commitConnectorDraft` don't exist.

- [ ] **Step 3: Implement in `src/tools/toolController.ts`**

Add after `DRAG_THRESHOLD`:

```ts
export const DEFAULT_ROUTING: "elbow" | "straight" = "elbow";
```

Add the draft interface to the existing `Draft` block:

```ts
export interface DraftConnector {
  kind: "connector";
  startId?: string;
  startPoint?: Point;
  beginPoint: Point;
  waypoints: Point[];
  endPoint: Point;
  routing: "elbow" | "straight";
  seed: number;
}

export type Draft = DraftShape | DraftFreehand | DraftConnector | null;
```

Add a pure data builder (exported for reuse in `draftToElement` and the controller):

```ts
export function connectorData(
  draft: DraftConnector,
  endId?: string,
  endPoint?: Point
): Record<string, any> {
  return {
    ...(draft.startId ? { startId: draft.startId } : { startPoint: draft.startPoint }),
    ...(endId ? { endId } : { endPoint: endPoint ?? draft.endPoint }),
    ...(draft.waypoints.length ? { waypoints: draft.waypoints } : {}),
    routing: draft.routing,
  };
}
```

Add a connector branch to `draftToElement` (inside the ternary, extend the `data` computation):

```ts
export function draftToElement(draft: NonNullable<Draft>, boardDefault: StyleMode): ElementData {
  const style: ElementStyle = { ...DRAFT_STYLE_BASE, styleMode: boardDefault };
  let elementType: string = "rectangle";
  let data: Record<string, any> = {};
  if (draft.kind === "freehand") {
    elementType = "freehand";
    data = freehandData(draft.points);
  } else if (draft.kind === "connector") {
    elementType = "connector";
    data = connectorData(draft, undefined, draft.endPoint);
  } else {
    elementType = draft.tool;
    data = dragShapeData(draft.tool, draft.startX, draft.startY, draft.endX, draft.endY);
  }
  return {
    id: "__draft__",
    elementType,
    data,
    style,
    transform: IDENTITY_TRANSFORM,
    order: Number.MAX_SAFE_INTEGER,
    seed: draft.seed,
  };
}
```

In the `ToolController` class:

- Add a field and include it in `get draft()`:

```ts
private draftConnector: DraftConnector | null = null;

get draft(): Draft {
  return this.draftShape ?? this.draftFreehand ?? this.draftConnector ?? null;
}
```

- Add a private hit-tolerance field and a shape-hit helper:

```ts
private hitTolerance = 8;

private shapeAt(pt: Point, tol: number): ElementData | undefined {
  const store = useEditorStore.getState();
  return [...store.elements]
    .reverse()
    .find((el) => el.elementType !== "connector" && hitElement(el, pt, tol, store.elements));
}
```

- In `beginTool`, store the tolerance and add the connector branch (before the eraser branch):

```ts
beginTool(tool: string, pt: Point, gridSnap: boolean, eraserTolerance = DEFAULT_ERASER_TOLERANCE): void {
  this.hitTolerance = eraserTolerance;
  const p = snapToGrid(pt, GRID_SIZE, gridSnap);

  if (tool === "connector") {
    if (this.draftConnector) {
      // Pending waypoint-mode connector: this click either glues the end to a
      // shape or appends a waypoint and keeps going.
      const hit = this.shapeAt(pt, this.hitTolerance);
      if (hit && hit.id !== this.draftConnector.startId) {
        const draft = this.draftConnector;
        this.draftConnector = null;
        this.commit(makeElement("connector", connectorData(draft, hit.id)));
      } else {
        this.draftConnector = {
          ...this.draftConnector,
          beginPoint: p,
          waypoints: [...this.draftConnector.waypoints, this.draftConnector.endPoint],
          endPoint: p,
        };
      }
      return;
    }
    const hit = this.shapeAt(pt, this.hitTolerance);
    this.draftConnector = {
      kind: "connector",
      startId: hit?.id,
      startPoint: hit ? undefined : p,
      beginPoint: p,
      waypoints: [],
      endPoint: p,
      routing: DEFAULT_ROUTING,
      seed: randomSeed(),
    };
    return;
  }
  // ...existing branches unchanged...
}
```

- In `updateTool`, add the connector branch:

```ts
if (this.draftConnector) {
  this.draftConnector = { ...this.draftConnector, endPoint: pt };
  return;
}
```

- In `endTool`, add the connector branch at the top (after clearing `this.erasing`):

```ts
if (this.draftConnector) {
  const draft = this.draftConnector;
  const moved =
    Math.hypot(draft.endPoint.x - draft.beginPoint.x, draft.endPoint.y - draft.beginPoint.y) >= DRAG_THRESHOLD;
  if (moved) {
    this.draftConnector = null;
    const hit = this.shapeAt(draft.endPoint, this.hitTolerance);
    if (hit && hit.id !== draft.startId) {
      this.commit(makeElement("connector", connectorData(draft, hit.id)));
    } else {
      this.commit(makeElement("connector", connectorData(draft, undefined, draft.endPoint)));
    }
  }
  // else: a click with no drag — keep the draft pending for waypoint mode.
  return;
}
```

- Add `commitConnectorDraft` and update `cancel`:

```ts
/** Commits a pending waypoint-mode connector as a free connector (double-click / Enter). */
commitConnectorDraft(): void {
  if (!this.draftConnector) return;
  const draft = this.draftConnector;
  this.draftConnector = null;
  this.commit(makeElement("connector", connectorData(draft, undefined, draft.endPoint)));
}

cancel(): void {
  this.draftShape = null;
  this.draftFreehand = null;
  this.draftConnector = null;
  this.erasing = false;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `bun test src/tools/toolController.test.ts`
Expected: PASS

- [ ] **Step 5: Gate + commit**

```bash
bun run check && bun test
git add src/tools/toolController.ts src/tools/toolController.test.ts
git commit -m "feat: connector tool with drag-glue and waypoint mode"
```

---

### Task 7: Connector tool UI wiring (Canvas, Toolbar, Properties, Help)

**Files:**
- Modify: `src/components/Canvas/Canvas.tsx`
- Modify: `src/components/Toolbar.tsx`
- Modify: `src/components/PropertiesPanel.tsx`
- Modify: `src/components/HelpOverlay.tsx`
- Test: `src/components/Toolbar.test.ts`

**Interfaces:**
- Consumes: `ToolController.commitConnectorDraft`/`cancel` (Task 6), `renderElement` `elements` param (Task 5), `elementWorldBounds`/`elementInMarquee` `elements` param (Task 4).
- Produces: fully interactive connector tool in the running app.

- [ ] **Step 1: Write the failing Toolbar test**

Append to `src/components/Toolbar.test.ts`:

```ts
it("advertises connector and image tools", () => {
  const ids = tools.map((t) => t.id);
  expect(ids).toContain("connector");
  expect(ids).toContain("image");
});
```

(Import `tools` from `./Toolbar`.)

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test src/components/Toolbar.test.ts`
Expected: FAIL — no `connector`/`image` tools.

- [ ] **Step 3: Add tools to `src/components/Toolbar.tsx`**

Append to the `tools` array:

```ts
  { id: "connector", label: "Connector", key: "C", icon: "∟" },
  { id: "image", label: "Image", key: "I", icon: "▧" },
```

- [ ] **Step 4: Wire the Canvas**

In `src/components/Canvas/Canvas.tsx`:

1. **Render loop** — pass `store.elements` to both `renderElement` calls (the element loop and the draft):

```ts
renderElement(ctx2, rc, el, resolveStyleMode(el, boardDefault), store.elements);
// ...
const draftEl = draftToElement(draft, boardDefault);
renderElement(ctx2, rc, draftEl, resolveStyleMode(draftEl, boardDefault), store.elements);
```

2. **Selection overlay** — pass `elements` into `drawSelectionOverlay` and skip handles for connectors:

```ts
drawSelectionOverlay(ctx2, store.elements, store.selectedElementIds, curZoom, store.elements);
```

Update the `drawSelectionOverlay` signature and body: after drawing the dashed bounds, `if (el.elementType === "connector") continue;` before the handle loop (connectors get a bounds box but no resize/rotate handles).

3. **Select-mode handle hit-test** — skip connectors and pass `elements`:

```ts
for (const el of store.elements) {
  if (!store.selectedElementIds.has(el.id)) continue;
  if (el.data.locked || el.data.hidden || el.elementType === "connector") continue;
  // ...unchanged...
}
```

4. **Select-mode element hit-test** — pass `elements`:

```ts
const hit = [...store.elements]
  .reverse()
  .find((el) => hitElement(el, invertTransformPoint(el.transform, world), tol, store.elements));
```

5. **Marquee on mouseup** — pass `elements`:

```ts
const hitIds = store.elements
  .filter((el) => !el.data.locked && !el.data.hidden && elementInMarquee(el, rect, store.elements))
  .map((el) => el.id);
```

6. **Cancellation on tool switch** — at the top of `handleMouseDown`, clear any pending connector draft when the active tool isn't the connector:

```ts
if (activeTool !== "connector") controllerRef.current.cancel();
```

7. **Double-click commit** — add an `onDoubleClick` handler on the `<canvas>`:

```tsx
onDoubleClick={() => controllerRef.current.commitConnectorDraft()}
```

8. **Enter commits the pending connector** — in `handleKeyDown`, add before the Escape handling:

```ts
if (e.key === "Enter") {
  controllerRef.current.commitConnectorDraft();
  return;
}
```

9. **Keyboard shortcuts** — in the `!mod` tool-shortcut block:

```ts
if (e.key === "c") setActiveTool("connector");
if (e.key === "i") setActiveTool("image");
```

10. **Escape already cancels** via `controllerRef.current.cancel()` (existing line).

- [ ] **Step 5: Add the PropertiesPanel routing toggle**

In `src/components/PropertiesPanel.tsx`, after the Mode row, add:

```tsx
{first.elementType === "connector" && (
  <PropertyRow label="Route">
    <Segmented
      value={first.data.routing ?? "elbow"}
      options={[
        { value: "elbow", label: "Elbow" },
        { value: "straight", label: "Straight" },
      ]}
      onChange={(v) =>
        selected.forEach((el) => updateElement(el.id, { data: { ...el.data, routing: v } }))
      }
    />
  </PropertyRow>
)}
```

- [ ] **Step 6: Add the HelpOverlay rows**

In `src/components/HelpOverlay.tsx`, after the Sticky row:

```ts
  ["C", "Connector"],
  ["I", "Image"],
```

- [ ] **Step 7: Run tests to verify they pass**

Run: `bun test src/components/Toolbar.test.ts`
Expected: PASS

- [ ] **Step 8: Gate + build + commit**

```bash
bun run check && bun test && bun run build
git add src/components/Canvas/Canvas.tsx src/components/Toolbar.tsx src/components/Toolbar.test.ts src/components/PropertiesPanel.tsx src/components/HelpOverlay.tsx
git commit -m "feat: wire connector tool into canvas, toolbar, properties, shortcuts"
```

---

### Task 8: Persistence round-trip for connectors

**Files:**
- Test: `src/utils/persistence.test.ts`

**Interfaces:**
- Consumes: `saveDoc`/`loadDoc` (existing), `makeElement` (existing).
- Produces: proof that connectors survive the local autosave round-trip (collab needs no change — data is plain JSON).

- [ ] **Step 1: Write the failing test**

Read `src/utils/persistence.test.ts` first to match its helpers. Append:

```ts
it("round-trips connectors and images through localStorage", () => {
  const connector = makeElement("connector", { startId: "a", endId: "b", waypoints: [{ x: 5, y: 5 }], routing: "elbow" });
  const image = makeElement("image", { x: 0, y: 0, width: 10, height: 10, src: "data:image/png;base64,AAA" });
  const board = makeDefaultBoard();
  saveDoc(board, [connector, image]);
  const doc = loadDoc();
  const loaded = doc!.elements;
  expect(loaded.find((e) => e.elementType === "connector")?.data).toMatchObject({
    startId: "a",
    endId: "b",
    waypoints: [{ x: 5, y: 5 }],
    routing: "elbow",
  });
  expect(loaded.find((e) => e.elementType === "image")?.data.src).toBe("data:image/png;base64,AAA");
});
```

(Match the existing test file's localStorage setup/teardown conventions.)

- [ ] **Step 2: Run test to verify it passes**

Run: `bun test src/utils/persistence.test.ts`
Expected: PASS (data is plain JSON — this guards against regression).

- [ ] **Step 3: Gate + commit**

```bash
bun run check && bun test
git add src/utils/persistence.test.ts
git commit -m "test: persist connectors and images round-trip"
```

---

## Phase B — Image tool

### Task 9: Image import utility (pure helpers + browser loader)

**Files:**
- Create: `src/utils/imageImport.ts`
- Create: `src/utils/imageImport.test.ts`

**Interfaces:**
- Produces:
  - `export const MAX_IMAGE_DIMENSION = 1200`
  - `export function computeDownscale(width: number, height: number, maxDim = MAX_IMAGE_DIMENSION): { width: number; height: number }`
  - `export function pickEncoding(hasAlpha: boolean): "image/jpeg" | "image/png"`
  - `export async function loadImageToDataUrl(file: File): Promise<{ dataUrl: string; width: number; height: number }>` (browser-only; jsdom can't decode — tested manually/visually)

- [ ] **Step 1: Write the failing tests**

Create `src/utils/imageImport.test.ts`:

```ts
import { describe, it, expect } from "vitest";
import { computeDownscale, pickEncoding, MAX_IMAGE_DIMENSION } from "./imageImport";

describe("computeDownscale", () => {
  it("keeps small images unchanged", () => {
    expect(computeDownscale(400, 300)).toEqual({ width: 400, height: 300 });
  });
  it("caps the longest edge at 1200, preserving aspect ratio", () => {
    expect(computeDownscale(2400, 1200)).toEqual({ width: 1200, height: 600 });
    expect(computeDownscale(1200, 2400)).toEqual({ width: 600, height: 1200 });
  });
  it("respects a custom max dimension", () => {
    expect(computeDownscale(2000, 1000, 100)).toEqual({ width: 100, height: 50 });
  });
});

describe("pickEncoding", () => {
  it("uses PNG for transparent sources and JPEG otherwise", () => {
    expect(pickEncoding(true)).toBe("image/png");
    expect(pickEncoding(false)).toBe("image/jpeg");
  });
});

describe("MAX_IMAGE_DIMENSION", () => {
  it("is 1200", () => {
    expect(MAX_IMAGE_DIMENSION).toBe(1200);
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `bun test src/utils/imageImport.test.ts`
Expected: FAIL — module doesn't exist.

- [ ] **Step 3: Implement `src/utils/imageImport.ts`**

```ts
export const MAX_IMAGE_DIMENSION = 1200;

/**
 * Scales an image so its longest edge is at most `maxDim`, preserving aspect
 * ratio. Returns integer dimensions, never smaller than 1×1.
 */
export function computeDownscale(
  width: number,
  height: number,
  maxDim = MAX_IMAGE_DIMENSION
): { width: number; height: number } {
  const longest = Math.max(width, height);
  if (longest <= maxDim) return { width, height };
  const scale = maxDim / longest;
  return {
    width: Math.max(1, Math.round(width * scale)),
    height: Math.max(1, Math.round(height * scale)),
  };
}

/** JPEG unless the source has an alpha channel, which JPEG would flatten. */
export function pickEncoding(hasAlpha: boolean): "image/jpeg" | "image/png" {
  return hasAlpha ? "image/png" : "image/jpeg";
}

/**
 * Decodes an image file, downscales it to at most MAX_IMAGE_DIMENSION on its
 * longest edge, and returns a data URL plus the final dimensions. Rejects
 * non-image files and decode failures. Browser-only (canvas + Image).
 */
export async function loadImageToDataUrl(file: File): Promise<{ dataUrl: string; width: number; height: number }> {
  if (!file.type.startsWith("image/")) throw new Error("Not an image file");
  const url = URL.createObjectURL(file);
  try {
    const img = await new Promise<HTMLImageElement>((resolve, reject) => {
      const el = new Image();
      el.onload = () => resolve(el);
      el.onerror = () => reject(new Error("Failed to decode image"));
      el.src = url;
    });
    const { width, height } = computeDownscale(img.naturalWidth || 0, img.naturalHeight || 0);
    const canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    const ctx = canvas.getContext("2d");
    if (!ctx) throw new Error("Canvas unavailable");
    ctx.drawImage(img, 0, 0, width, height);
    const hasAlpha = detectAlpha(ctx, width, height);
    return { dataUrl: canvas.toDataURL(pickEncoding(hasAlpha), 0.85), width, height };
  } finally {
    URL.revokeObjectURL(url);
  }
}

/** True if any sampled pixel has alpha < 255. */
function detectAlpha(ctx: CanvasRenderingContext2D, w: number, h: number): boolean {
  try {
    const data = ctx.getImageData(0, 0, w, h).data;
    for (let i = 3; i < data.length; i += 4) {
      if (data[i] < 255) return true;
    }
    return false;
  } catch {
    return false; // tainted canvas — treat as opaque
  }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `bun test src/utils/imageImport.test.ts`
Expected: PASS

- [ ] **Step 5: Gate + commit**

```bash
bun run check && bun test
git add src/utils/imageImport.ts src/utils/imageImport.test.ts
git commit -m "feat: image import utility with downscale"
```

---

### Task 10: Image placement (real rendering + tool + DnD + paste)

**Files:**
- Modify: `src/render/renderElement.ts`
- Modify: `src/components/Canvas/Canvas.tsx`
- Modify: `src/components/Toolbar.tsx` (already added in Task 7 — no change here)
- Test: none unit-level (renderer/picker are visual); rely on Task 9 helpers.

**Interfaces:**
- Consumes: `loadImageToDataUrl` (Task 9).
- Produces: `"image"` elements with `src` data URLs render real pixels; the image tool, file drag-and-drop onto the canvas, and clipboard paste all insert images.

- [ ] **Step 1: Implement real image rendering in `src/render/renderElement.ts`**

Add a module-level cache and a render function, and switch the `image` case to it:

```ts
const imageCache = new Map<string, HTMLImageElement>();

function ensureImage(src: string): HTMLImageElement {
  let img = imageCache.get(src);
  if (!img) {
    img = new Image();
    img.src = src;
    imageCache.set(src, img);
  }
  return img;
}

function renderImage(ctx: CanvasRenderingContext2D, el: ElementData): void {
  const d = el.data as { x?: number; y?: number; width?: number; height?: number; src?: string };
  const x = d.x ?? 0;
  const y = d.y ?? 0;
  const width = d.width ?? 0;
  const height = d.height ?? 0;
  if (d.src) {
    const img = ensureImage(d.src);
    if (img.complete && img.naturalWidth > 0) {
      ctx.drawImage(img, x, y, width, height);
      return;
    }
  }
  renderImagePlaceholder(ctx, el);
}
```

Change the `image` case in `renderElement` from `renderImagePlaceholder(ctx, el)` to `renderImage(ctx, el)`.

- [ ] **Step 2: Implement the image tool in `Canvas.tsx`**

Add refs and a hidden file input at the top of the component:

```tsx
const fileInputRef = useRef<HTMLInputElement>(null);
const pendingImageRef = useRef<Point | null>(null);
```

In `handleMouseDown`, before the final `controllerRef.current.beginTool(...)`, add:

```ts
if (activeTool === "image") {
  const p = snapToGrid(world, GRID_SIZE, gridSnap);
  pendingImageRef.current = p;
  fileInputRef.current?.click();
  return;
}
```

Add an async insert helper and the file-input change handler inside the component:

```ts
const insertImageAt = useCallback(async (file: File, world: Point) => {
  try {
    const { dataUrl, width, height } = await loadImageToDataUrl(file);
    const store = useEditorStore.getState();
    const el = makeElement("image", {
      x: world.x - width / 2,
      y: world.y - height / 2,
      width,
      height,
      src: dataUrl,
    });
    store.addElement(el);
    store.setSelection([el.id]);
  } catch (err) {
    console.error("Failed to place image:", err);
  }
}, []);

const onImageFileChange = useCallback(
  (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    const pt = pendingImageRef.current;
    e.target.value = "";
    if (file && pt) void insertImageAt(file, pt);
  },
  [insertImageAt]
);
```

Add drag-and-drop handlers:

```ts
const handleDragOver = (e: React.DragEvent) => e.preventDefault();

const handleDrop = useCallback(
  (e: React.DragEvent) => {
    e.preventDefault();
    const file = e.dataTransfer.files?.[0];
    if (!file || !file.type.startsWith("image/")) return;
    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const world = screenToWorld(e.clientX - rect.left, e.clientY - rect.top);
    void insertImageAt(file, world);
  },
  [screenToWorld, insertImageAt]
);
```

Add a paste handler and register it in the keydown `useEffect` (window listener):

```ts
const onPaste = useCallback(
  (e: ClipboardEvent) => {
    const items = e.clipboardData?.items;
    if (!items) return;
    const item = Array.from(items).find((i) => i.type.startsWith("image/"));
    const file = item?.getAsFile();
    if (!file) return;
    const store = useEditorStore.getState();
    const world = { x: window.innerWidth / 2 / store.zoom - store.pan.x, y: window.innerHeight / 2 / store.zoom - store.pan.y };
    void insertImageAt(file, world);
  },
  [insertImageAt]
);
```

In the keydown/keyup `useEffect`, add the paste listener:

```ts
window.addEventListener("paste", onPaste);
// ...cleanup: window.removeEventListener("paste", onPaste);
```

Import `loadImageToDataUrl` from `../../utils/imageImport` and `makeElement` from `../../stores/model` (add if missing).

Render the hidden input inside the returned JSX (alongside the canvas):

```tsx
<input ref={fileInputRef} type="file" accept="image/*" className="hidden" onChange={onImageFileChange} />
```

Wire the drop/dragover onto the `<canvas>` element:

```tsx
onDragOver={handleDragOver}
onDrop={handleDrop}
```

- [ ] **Step 3: Gate + build + commit**

```bash
bun run check && bun test && bun run build
git add src/render/renderElement.ts src/components/Canvas/Canvas.tsx
git commit -m "feat: image tool with file picker, drag-drop, and paste"
```

---

## Phase C — Templates

### Task 11: Template data definitions

**Files:**
- Create: `src/templates/index.ts`
- Create: `src/templates/templates.test.ts`

**Interfaces:**
- Consumes: `makeElement` (existing), `measureTextSize` (existing, from `../tools/toolController`).
- Produces:
  - `export interface Template { id: string; name: string; description: string; icon: string; build(): ElementData[] }`
  - `export const templates: Template[]` — Flowchart, Mind Map, Org Chart, Grid, Kanban. Flowchart + Org Chart include glued connectors.

- [ ] **Step 1: Write the failing tests**

Create `src/templates/templates.test.ts`:

```ts
import { describe, it, expect } from "vitest";
import { templates } from "./index";

describe("templates", () => {
  it("exposes the full curated set", () => {
    const ids = templates.map((t) => t.id).sort();
    expect(ids).toEqual(["flowchart", "grid", "kanban", "mindmap", "orgchart"]);
  });

  it("every template builds elements with unique ids", () => {
    for (const t of templates) {
      const els = t.build();
      expect(els.length).toBeGreaterThan(0);
      const ids = new Set(els.map((e) => e.id));
      expect(ids.size).toBe(els.length);
    }
  });

  it("connectors in templates reference shapes that exist in the same build", () => {
    for (const t of templates) {
      const els = t.build();
      const ids = new Set(els.map((e) => e.id));
      for (const el of els) {
        if (el.elementType !== "connector") continue;
        const d = el.data as any;
        expect(ids.has(d.startId)).toBe(true);
        expect(ids.has(d.endId)).toBe(true);
      }
    }
  });

  it("text elements get a measured non-zero size", () => {
    for (const t of templates) {
      for (const el of t.build()) {
        if (el.elementType === "text") {
          expect(el.data.width ?? 0).toBeGreaterThan(0);
          expect(el.data.height ?? 0).toBeGreaterThan(0);
        }
      }
    }
  });

  it("flowchart and orgchart use connectors", () => {
    for (const id of ["flowchart", "orgchart"]) {
      const els = templates.find((t) => t.id === id)!.build();
      expect(els.some((e) => e.elementType === "connector")).toBe(true);
    }
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `bun test src/templates/templates.test.ts`
Expected: FAIL — module doesn't exist.

- [ ] **Step 3: Implement `src/templates/index.ts`**

```ts
import { makeElement, type ElementData } from "../stores/model";
import { measureTextSize } from "../tools/toolController";

export interface Template {
  id: string;
  name: string;
  description: string;
  icon: string;
  build(): ElementData[];
}

/** Box helper: rectangle at x,y with given size, fill, and an optional label. */
function box(x: number, y: number, w: number, h: number, fill: string, stroke: string, label?: string): ElementData[] {
  const rect = makeElement("rectangle", { x, y, width: w, height: h }, { fill, stroke, radius: 4 });
  const els: ElementData[] = [rect];
  if (label) {
    const text = makeElement("text", { x: x + 8, y: y + 6, text: label }, { fontSize: 14, stroke: "#ffffff" });
    const size = measureTextSize(label, text.style.fontSize, text.style.fontFamily);
    text.data.width = size.width;
    text.data.height = size.height;
    els.push(text);
  }
  return els;
}

/** Glued elbow connector between two existing element ids. */
function link(fromId: string, toId: string): ElementData {
  return makeElement("connector", { startId: fromId, endId: toId, routing: "elbow" });
}

const flowchart: Template = {
  id: "flowchart",
  name: "Flowchart",
  description: "Start → Process → Decision with connectors",
  icon: "⇢",
  build: () => {
    const [start] = box(-60, -120, 120, 40, "#3b82f6", "#1d4ed8", "Start");
    const [process] = box(-60, -40, 120, 40, "#10b981", "#047857", "Process");
    const [decision] = box(-60, 40, 120, 40, "#f59e0b", "#b45309", "Decision");
    const [end] = box(-60, 120, 120, 40, "#ef4444", "#b91c1c", "End");
    return [start, process, decision, end, link(start.id, process.id), link(process.id, decision.id), link(decision.id, end.id)];
  },
};

const mindmap: Template = {
  id: "mindmap",
  name: "Mind Map",
  description: "Central idea with radiating branches",
  icon: "✳",
  build: () => {
    const center = makeElement("ellipse", { x: -80, y: -40, width: 160, height: 80 }, { fill: "#8b5cf6", stroke: "#6d28d9" });
    const [b1] = box(140, -160, 160, 48, "#60a5fa", "#1d4ed8", "Branch 1");
    const [b2] = box(140, -40, 160, 48, "#34d399", "#047857", "Branch 2");
    const [b3] = box(140, 80, 160, 48, "#fbbf24", "#b45309", "Branch 3");
    return [center, b1, b2, b3, link(center.id, b1.id), link(center.id, b2.id), link(center.id, b3.id)];
  },
};

const orgchart: Template = {
  id: "orgchart",
  name: "Org Chart",
  description: "A root with three direct reports",
  icon: "▤",
  build: () => {
    const [root] = box(-80, -140, 160, 48, "#6366f1", "#4338ca", "CEO");
    const [a] = box(-220, 20, 160, 48, "#22d3ee", "#0e7490", "Team A");
    const [b] = box(-60, 20, 160, 48, "#f472b6", "#be185d", "Team B");
    const [c] = box(100, 20, 160, 48, "#a3e635", "#4d7c0f", "Team C");
    return [root, a, b, c, link(root.id, a.id), link(root.id, b.id), link(root.id, c.id)];
  },
};

const grid: Template = {
  id: "grid",
  name: "Grid",
  description: "A 4×4 canvas grid",
  icon: "▦",
  build: () => {
    const els: ElementData[] = [];
    for (let r = 0; r < 4; r++) {
      for (let c = 0; c < 4; c++) {
        els.push(makeElement("rectangle", { x: c * 70 - 140, y: r * 70 - 140, width: 60, height: 60 }, { fill: "none", stroke: "#64748b" }));
      }
    }
    return els;
  },
};

const kanban: Template = {
  id: "kanban",
  name: "Kanban",
  description: "Three columns with sticky cards",
  icon: "▥",
  build: () => {
    const [todo] = box(-240, -120, 160, 40, "#3b82f6", "#1d4ed8", "To Do");
    const [doing] = box(-40, -120, 160, 40, "#f59e0b", "#b45309", "Doing");
    const [done] = box(160, -120, 160, 40, "#10b981", "#047857", "Done");
    const cards = [
      makeElement("sticky", { x: -230, y: -60, width: 140, height: 70, text: "Card 1" }, { stroke: "#d97706" }),
      makeElement("sticky", { x: -30, y: -60, width: 140, height: 70, text: "Card 2" }, { stroke: "#d97706" }),
      makeElement("sticky", { x: 170, y: -60, width: 140, height: 70, text: "Card 3" }, { stroke: "#d97706" }),
    ];
    return [todo, doing, done, ...cards];
  },
};

export const templates: Template[] = [flowchart, mindmap, orgchart, grid, kanban];
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `bun test src/templates/templates.test.ts`
Expected: PASS

- [ ] **Step 5: Gate + commit**

```bash
bun run check && bun test
git add src/templates/index.ts src/templates/templates.test.ts
git commit -m "feat: define template data set"
```

---

### Task 12: Template insertion (one undo step)

**Files:**
- Create: `src/templates/insertTemplate.ts`
- Test: `src/templates/templates.test.ts` (append)

**Interfaces:**
- Consumes: `Template` (Task 11), `elementBounds` (Task 2), `translateElement` (Task 4), `useEditorStore`.
- Produces:
  - `export function insertTemplate(template: Template, viewCenter: Point): string[]` — returns the inserted element ids.

- [ ] **Step 1: Write the failing test**

Append to `src/templates/templates.test.ts`:

```ts
import { useEditorStore } from "../stores/useEditorStore";
import { insertTemplate } from "./insertTemplate";

describe("insertTemplate", () => {
  it("adds all elements at the viewport center as one undo step and selects them", () => {
    useEditorStore.setState({ elements: [], selectedElementIds: new Set(), undoStack: [], redoStack: [] });
    const t = templates.find((t) => t.id === "flowchart")!;
    const ids = insertTemplate(t, { x: 500, y: 400 });
    const store = useEditorStore.getState();
    const built = t.build();
    expect(store.elements.length).toBe(built.length);
    expect(store.undoStack.length).toBe(1);
    expect([...store.selectedElementIds].sort()).toEqual([...ids].sort());
    // the whole template is centered on (500,400)
    const xs = store.elements.map((e) => {
      const b = elementBounds(e, store.elements);
      return [b.x, b.x + b.width];
    }).flat();
    const ys = store.elements.map((e) => {
      const b = elementBounds(e, store.elements);
      return [b.y, b.y + b.height];
    }).flat();
    expect((Math.min(...xs) + Math.max(...xs)) / 2).toBeCloseTo(500, 4);
    expect((Math.min(...ys) + Math.max(...ys)) / 2).toBeCloseTo(400, 4);
  });
});
```

(Import `elementBounds` from `../render/geometry`.)

- [ ] **Step 2: Run test to verify it fails**

Run: `bun test src/templates/templates.test.ts`
Expected: FAIL — `insertTemplate` doesn't exist.

- [ ] **Step 3: Implement `src/templates/insertTemplate.ts`**

```ts
import type { ElementData } from "../stores/model";
import { useEditorStore } from "../stores/useEditorStore";
import { elementBounds, type Bounds, type Point } from "../render/geometry";
import { translateElement } from "../tools/selection";
import type { Template } from "./index";

/** AABB of a set of built template elements (connectors resolve via the set). */
function templateBounds(els: ElementData[]): Bounds {
  const xs: number[] = [];
  const ys: number[] = [];
  for (const el of els) {
    const b = elementBounds(el, els);
    xs.push(b.x, b.x + b.width);
    ys.push(b.y, b.y + b.height);
  }
  return {
    x: Math.min(...xs),
    y: Math.min(...ys),
    width: Math.max(...xs) - Math.min(...xs),
    height: Math.max(...ys) - Math.min(...ys),
  };
}

/**
 * Offsets a template element into place. Connectors keep their startId/endId
 * glue but shift their stored anchor points so the whole template moves as one
 * (translateElement alone skips glued connectors by design).
 */
function offsetForTemplate(el: ElementData, dx: number, dy: number): ElementData {
  if (el.elementType === "connector") {
    const c = el.data as any;
    return {
      ...el,
      data: {
        ...c,
        startPoint: c.startPoint ? { x: c.startPoint.x + dx, y: c.startPoint.y + dy } : undefined,
        endPoint: c.endPoint ? { x: c.endPoint.x + dx, y: c.endPoint.y + dy } : undefined,
        waypoints: (c.waypoints ?? []).map((w: Point) => ({ x: w.x + dx, y: w.y + dy })),
      },
    };
  }
  return translateElement(el, dx, dy);
}

/**
 * Builds the template, centers it on `viewCenter`, and appends all elements to
 * the board as a single undo step, selecting them. Returns the new element ids.
 */
export function insertTemplate(template: Template, viewCenter: Point): string[] {
  const store = useEditorStore.getState();
  const built = template.build();
  const b = templateBounds(built);
  const dx = viewCenter.x - (b.x + b.width / 2);
  const dy = viewCenter.y - (b.y + b.height / 2);
  const placed = built.map((el) => offsetForTemplate(el, dx, dy));
  const base = store.elements.length;
  placed.forEach((el, i) => {
    el.order = base + i;
  });
  store.pushHistory();
  store.setElementsLive([...store.elements, ...placed]);
  const ids = placed.map((el) => el.id);
  store.setSelection(ids);
  return ids;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `bun test src/templates/templates.test.ts`
Expected: PASS

- [ ] **Step 5: Gate + commit**

```bash
bun run check && bun test
git add src/templates/insertTemplate.ts src/templates/templates.test.ts
git commit -m "feat: insert templates as a single undo step"
```

---

### Task 13: TemplatesPanel + app wiring

**Files:**
- Create: `src/components/TemplatesPanel.tsx`
- Modify: `src/stores/useEditorStore.ts:45` (sidebar union)
- Modify: `src/App.tsx`

**Interfaces:**
- Consumes: `templates` (Task 11), `insertTemplate` (Task 12), `useEditorStore`.
- Produces: a Templates sidebar panel reachable from the bottom bar.

- [ ] **Step 1: Implement `src/components/TemplatesPanel.tsx`**

```tsx
import { templates, type Template } from "../templates/index";
import { insertTemplate } from "../templates/insertTemplate";
import { useEditorStore } from "../stores/useEditorStore";

export default function TemplatesPanel() {
  const setSidebar = useEditorStore((s) => s.setSidebar);

  const insert = (t: Template) => {
    const st = useEditorStore.getState();
    const viewCenter = {
      x: window.innerWidth / 2 / st.zoom - st.pan.x,
      y: window.innerHeight / 2 / st.zoom - st.pan.y,
    };
    insertTemplate(t, viewCenter);
    setSidebar(null);
  };

  return (
    <div className="flex flex-col h-full p-2 gap-2">
      <div className="text-xs font-semibold text-zinc-400 uppercase">Templates</div>
      <div className="text-[11px] text-zinc-600">Insert a ready-made diagram at the viewport center.</div>
      <div className="flex flex-col gap-2 overflow-y-auto">
        {templates.map((t) => (
          <button
            key={t.id}
            onClick={() => insert(t)}
            className="flex items-start gap-3 p-2 rounded border border-zinc-700 bg-zinc-800 hover:bg-zinc-700 text-left"
          >
            <span className="text-xl text-blue-400">{t.icon}</span>
            <span>
              <span className="block text-sm font-medium text-zinc-200">{t.name}</span>
              <span className="block text-[11px] text-zinc-500">{t.description}</span>
            </span>
          </button>
        ))}
      </div>
    </div>
  );
}
```

- [ ] **Step 2: Extend the sidebar union**

In `src/stores/useEditorStore.ts`, update both the `sidebar` field type and `setSidebar` signature:

```ts
sidebar: "layers" | "properties" | "boards" | "ai" | "templates" | null;
setSidebar: (s: "layers" | "properties" | "boards" | "ai" | "templates" | null) => void;
```

- [ ] **Step 3: Wire into `App.tsx`**

Import `TemplatesPanel`. In the panel render block, add:

```tsx
{sidebar === "templates" && (
  <div className="w-64 border-l border-zinc-800 bg-zinc-900">
    <TemplatesPanel />
  </div>
)}
```

Add a bottom-bar button after the AI button:

```tsx
<button
  onClick={() => setSidebar(sidebar === "templates" ? null : "templates")}
  className={`hover:text-zinc-300 ${sidebar === "templates" ? "text-blue-400" : ""}`}
>
  Templates
</button>
```

- [ ] **Step 4: Gate + build + commit**

```bash
bun run check && bun test && bun run build
git add src/components/TemplatesPanel.tsx src/stores/useEditorStore.ts src/App.tsx
git commit -m "feat: templates sidebar panel"
```

---

## Self-Review (checked)

1. **Spec coverage:**
   - Connectors: Task 1 (type), 2 (routing), 3 (hit), 4 (selection), 5 (render+SVG), 6 (tool), 7 (UI), 8 (persistence). ✓
   - Images: Task 9 (import), 10 (placement/render/DnD/paste). ✓
   - Templates: Task 11 (data), 12 (insert), 13 (panel). ✓
   - Collab: connector/image data is plain JSON — `writeElements` needs no change (verified by Task 8 + existing `yElements.test.ts`). ✓
2. **Placeholder scan:** all steps contain concrete code or exact instructions. ✓
3. **Type consistency:** `routeConnector(el, elements)`, `hitElement(el, p, tol, elements?)`, `elementBounds(el, elements?)`, `elementWorldBounds(el, elements?)`, `elementInMarquee(el, rect, elements?)`, `renderElement(ctx, rc, el, mode, elements?)`, `svgElement(el, mode, elements?)`, `translateElement` connector branch, `DraftConnector`, `connectorData(draft, endId?, endPoint?)`, `DEFAULT_ROUTING`, `commitConnectorDraft()`, `insertTemplate(template, viewCenter)`, `computeDownscale`, `pickEncoding`, `loadImageToDataUrl` — names/params consistent across tasks. ✓
