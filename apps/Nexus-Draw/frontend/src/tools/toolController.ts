import { makeElement, type ElementData, type ElementStyle, type StyleMode } from "../stores/model";
import { useEditorStore } from "../stores/useEditorStore";
import { hitElement, type Point } from "../render/hitTest";

/** World-space grid the editor snaps to when `board.gridSnap` is on. */
export const GRID_SIZE = 40;

/** Below this drag distance (world px), a shape-drag is treated as a click. */
const DRAG_THRESHOLD = 2;
const DEFAULT_STICKY_WIDTH = 200;
const DEFAULT_STICKY_HEIGHT = 150;
const DEFAULT_ERASER_TOLERANCE = 8;

export type DragShapeTool = "rectangle" | "ellipse" | "line" | "arrow" | "sticky";
const DRAG_SHAPE_TOOLS: ReadonlySet<string> = new Set<DragShapeTool>([
  "rectangle",
  "ellipse",
  "line",
  "arrow",
  "sticky",
]);

export function isDragShapeTool(tool: string): tool is DragShapeTool {
  return DRAG_SHAPE_TOOLS.has(tool);
}

function randomSeed(): number {
  return Math.floor(Math.random() * 2 ** 31);
}

/** Snaps a world point to the grid when `enabled`; passes it through unchanged otherwise. */
export function snapToGrid(pt: Point, gridSize: number, enabled: boolean): Point {
  if (!enabled) return pt;
  return {
    x: Math.round(pt.x / gridSize) * gridSize,
    y: Math.round(pt.y / gridSize) * gridSize,
  };
}

/** Element `data` for a drag-created box/line/arrow/sticky, from its two drag corners. */
export function dragShapeData(
  tool: DragShapeTool,
  x1: number,
  y1: number,
  x2: number,
  y2: number
): Record<string, any> {
  if (tool === "line" || tool === "arrow") {
    return { x1, y1, x2, y2 };
  }
  const data: Record<string, any> = {
    x: Math.min(x1, x2),
    y: Math.min(y1, y2),
    width: Math.abs(x2 - x1),
    height: Math.abs(y2 - y1),
  };
  if (tool === "sticky") data.text = "";
  return data;
}

/** Element `data` for a default-size sticky placed by a click (no drag). */
export function defaultStickyData(x: number, y: number): Record<string, any> {
  return { x, y, width: DEFAULT_STICKY_WIDTH, height: DEFAULT_STICKY_HEIGHT, text: "" };
}

/** Element `data` for a freehand stroke from accumulated `[x, y, pressure]` samples. */
export function freehandData(points: number[][]): Record<string, any> {
  return { points };
}

/** Element `data` for a text element placed at a point. */
export function textData(x: number, y: number, text: string): Record<string, any> {
  return { x, y, text };
}

export interface DraftShape {
  kind: "shape";
  tool: DragShapeTool;
  startX: number;
  startY: number;
  endX: number;
  endY: number;
  seed: number;
}

export interface DraftFreehand {
  kind: "freehand";
  points: number[][];
  seed: number;
}

export type Draft = DraftShape | DraftFreehand | null;

const DRAFT_STYLE_BASE: Omit<ElementStyle, "styleMode"> = {
  stroke: "#60a5fa",
  fill: "none",
  strokeWidth: 2,
  strokeStyle: "solid",
  opacity: 0.85,
  radius: 8,
  fontFamily: "ui-sans-serif, system-ui",
  fontSize: 20,
  textAlign: "left",
};

const IDENTITY_TRANSFORM = { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 };

/** Renders the in-progress draft as an (uncommitted) ElementData, for live preview. */
export function draftToElement(draft: NonNullable<Draft>, boardDefault: StyleMode): ElementData {
  const style: ElementStyle = { ...DRAFT_STYLE_BASE, styleMode: boardDefault };
  const data =
    draft.kind === "freehand"
      ? freehandData(draft.points)
      : dragShapeData(draft.tool, draft.startX, draft.startY, draft.endX, draft.endY);

  return {
    id: "__draft__",
    elementType: draft.kind === "freehand" ? "freehand" : draft.tool,
    data,
    style,
    transform: IDENTITY_TRANSFORM,
    order: Number.MAX_SAFE_INTEGER,
    seed: draft.seed,
  };
}

/**
 * Owns the in-progress draft for drag/freehand/eraser tools and commits finished
 * elements to the store. `beginTool`/`updateTool`/`endTool` are pointer-glue —
 * called from Canvas's onPointerDown/Move/Up — and are safe to call for any tool,
 * including ones (select, hand, text) this controller doesn't drive; they no-op.
 * The `text` tool is handled by Canvas directly (it needs a DOM textarea overlay)
 * but commits through `commitText` below so element construction stays in one place.
 */
export class ToolController {
  private draftShape: DraftShape | null = null;
  private draftFreehand: DraftFreehand | null = null;
  private erasing = false;

  /** The current in-progress draft, for the caller to render as a live preview. */
  get draft(): Draft {
    return this.draftShape ?? this.draftFreehand ?? null;
  }

  beginTool(tool: string, pt: Point, gridSnap: boolean, eraserTolerance = DEFAULT_ERASER_TOLERANCE): void {
    const p = snapToGrid(pt, GRID_SIZE, gridSnap);

    if (isDragShapeTool(tool)) {
      this.draftShape = { kind: "shape", tool, startX: p.x, startY: p.y, endX: p.x, endY: p.y, seed: randomSeed() };
      return;
    }
    if (tool === "pen") {
      this.draftFreehand = { kind: "freehand", points: [[p.x, p.y, 0.5]], seed: randomSeed() };
      return;
    }
    if (tool === "eraser") {
      this.erasing = true;
      this.eraseAt(pt, eraserTolerance);
    }
  }

  updateTool(pt: Point, gridSnap: boolean, eraserTolerance = DEFAULT_ERASER_TOLERANCE): void {
    if (this.draftShape) {
      const p = snapToGrid(pt, GRID_SIZE, gridSnap);
      this.draftShape = { ...this.draftShape, endX: p.x, endY: p.y };
      return;
    }
    if (this.draftFreehand) {
      // Freehand strokes stay unsnapped point-by-point — snapping every sample would
      // make the stroke blocky and defeat the point of a "freehand" tool.
      this.draftFreehand.points.push([pt.x, pt.y, 0.5]);
      return;
    }
    if (this.erasing) {
      this.eraseAt(pt, eraserTolerance);
    }
  }

  endTool(): void {
    this.erasing = false;

    if (this.draftShape) {
      const draft = this.draftShape;
      this.draftShape = null;
      const dx = draft.endX - draft.startX;
      const dy = draft.endY - draft.startY;
      if (Math.abs(dx) < DRAG_THRESHOLD && Math.abs(dy) < DRAG_THRESHOLD) {
        if (draft.tool === "sticky") {
          this.commit(makeElement("sticky", defaultStickyData(draft.startX, draft.startY)));
        }
        return;
      }
      this.commit(
        makeElement(draft.tool, dragShapeData(draft.tool, draft.startX, draft.startY, draft.endX, draft.endY))
      );
      return;
    }

    if (this.draftFreehand) {
      const draft = this.draftFreehand;
      this.draftFreehand = null;
      if (draft.points.length < 2) return;
      this.commit(makeElement("freehand", freehandData(draft.points)));
    }
  }

  /** Cancels any in-progress draft without committing (e.g. Escape, tool switch). */
  cancel(): void {
    this.draftShape = null;
    this.draftFreehand = null;
    this.erasing = false;
  }

  /** Removes the topmost element under `pt` (world space), within `tol`. Used by the eraser tool. */
  eraseAt(pt: Point, tol: number): void {
    const store = useEditorStore.getState();
    const hit = [...store.elements].reverse().find((el) => hitElement(el, pt, tol));
    if (hit) store.removeElement(hit.id);
  }

  /** Commits a text element typed into the overlay; a left-empty textarea cancels instead. */
  commitText(x: number, y: number, text: string): void {
    if (text.length === 0) return;
    this.commit(makeElement("text", textData(x, y, text)));
  }

  private commit(el: ElementData): void {
    const store = useEditorStore.getState();
    el.order = store.elements.length;
    store.addElement(el);
  }
}
