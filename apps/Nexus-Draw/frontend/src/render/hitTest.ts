import type { ElementData } from "../stores/model";
import { BOX_TYPES, type Bounds, type Point, distToSegment, elementBounds } from "./geometry";

export { elementBounds };
export type { Bounds, Point };

const ROTATE_HANDLE_OFFSET = 24;

function pointInBoxWithTol(p: Point, b: Bounds, tol: number): boolean {
  return (
    p.x >= b.x - tol &&
    p.x <= b.x + b.width + tol &&
    p.y >= b.y - tol &&
    p.y <= b.y + b.height + tol
  );
}

/** True if point p hits element el, within tolerance tol (in element-local coords). */
export function hitElement(el: ElementData, p: Point, tol: number): boolean {
  const d = el.data as Record<string, any>;
  // Locked and hidden elements are not pickable: a click falls through to
  // whatever sits underneath them. Without this, locking only stops the
  // properties panel, not dragging, and hidden elements stay grabbable.
  if (d.locked || d.hidden) return false;
  const strokeWidth = el.style?.strokeWidth ?? 0;

  if (BOX_TYPES.has(el.elementType)) {
    return pointInBoxWithTol(p, elementBounds(el), tol);
  }

  if (el.elementType === "line" || el.elementType === "arrow") {
    const a = { x: d.x1 ?? 0, y: d.y1 ?? 0 };
    const b = { x: d.x2 ?? 0, y: d.y2 ?? 0 };
    return distToSegment(p, a, b) <= tol + strokeWidth;
  }

  if (el.elementType === "freehand") {
    const points: number[][] = d.points ?? [];
    if (points.length === 0) return false;
    if (points.length === 1) {
      const [px, py] = points[0];
      return distToSegment(p, { x: px, y: py }, { x: px, y: py }) <= tol + strokeWidth;
    }
    let minDist = Infinity;
    for (let i = 0; i < points.length - 1; i++) {
      const [ax, ay] = points[i];
      const [bx, by] = points[i + 1];
      const dist = distToSegment(p, { x: ax, y: ay }, { x: bx, y: by });
      if (dist < minDist) minDist = dist;
    }
    return minDist <= tol + strokeWidth;
  }

  return pointInBoxWithTol(p, elementBounds(el), tol);
}

/** True if el's bounds are fully contained within rect (the marquee selection box). */
export function hitInMarquee(el: ElementData, rect: Bounds): boolean {
  const d = el.data as Record<string, any>;
  if (d.locked || d.hidden) return false;
  const b = elementBounds(el);
  return (
    b.x >= rect.x &&
    b.y >= rect.y &&
    b.x + b.width <= rect.x + rect.width &&
    b.y + b.height <= rect.y + rect.height
  );
}

/** The 8 box resize handles plus a rotate handle above top-center, for bounds b. */
export function resizeHandles(b: Bounds): { id: string; x: number; y: number }[] {
  const { x, y, width, height } = b;
  const midX = x + width / 2;
  const midY = y + height / 2;
  const right = x + width;
  const bottom = y + height;

  return [
    { id: "nw", x, y },
    { id: "n", x: midX, y },
    { id: "ne", x: right, y },
    { id: "e", x: right, y: midY },
    { id: "se", x: right, y: bottom },
    { id: "s", x: midX, y: bottom },
    { id: "sw", x, y: bottom },
    { id: "w", x, y: midY },
    { id: "rotate", x: midX, y: y - ROTATE_HANDLE_OFFSET },
  ];
}
