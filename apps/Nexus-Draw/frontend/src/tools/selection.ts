import type { ElementData } from "../stores/model";
import { BOX_TYPES, elementBounds, type Bounds, type Point } from "../render/geometry";
import { resizeHandles } from "../render/hitTest";

export type Transform = ElementData["transform"];

const IDENTITY: Transform = { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 };

/** True if m is (numerically) the identity matrix. */
function isIdentity(m: Transform): boolean {
  return m.a === IDENTITY.a && m.b === IDENTITY.b && m.c === IDENTITY.c && m.d === IDENTITY.d && m.e === IDENTITY.e && m.f === IDENTITY.f;
}

/** Applies affine matrix m to point p (element-local -> parent/world space). */
export function applyTransform(m: Transform, p: Point): Point {
  return { x: m.a * p.x + m.c * p.y + m.e, y: m.b * p.x + m.d * p.y + m.f };
}

/**
 * Applies the inverse of affine matrix m to point p (parent/world -> element-local
 * space). Used to hit-test a rotated element/handle against a world-space pointer
 * position without having to rotate the element's own data.
 */
export function invertTransformPoint(m: Transform, p: Point): Point {
  if (isIdentity(m)) return p;
  const det = m.a * m.d - m.b * m.c;
  if (det === 0) return p;
  const dx = p.x - m.e;
  const dy = p.y - m.f;
  return {
    x: (m.d * dx - m.c * dy) / det,
    y: (-m.b * dx + m.a * dy) / det,
  };
}

/** The element's current rotation angle (radians), extracted from its transform. */
export function currentRotation(el: ElementData): number {
  return Math.atan2(el.transform.b, el.transform.a);
}

/**
 * Sets el's transform to a rotation of angleRad about its own (element-local,
 * untransformed) center. Pure: the matrix is derived entirely from el's current
 * local bounds plus the target angle, so calling it again after a move/resize
 * (which recompute the center from the new bounds) keeps the pivot correct
 * instead of orbiting around a stale position.
 */
export function rotateElementTransform(el: ElementData, angleRad: number): ElementData {
  const b = elementBounds(el);
  const cx = b.x + b.width / 2;
  const cy = b.y + b.height / 2;
  const cos = Math.cos(angleRad);
  const sin = Math.sin(angleRad);
  const transform: Transform = {
    a: cos,
    b: sin,
    c: -sin,
    d: cos,
    e: cx * (1 - cos) + cy * sin,
    f: cy * (1 - cos) - cx * sin,
  };
  return { ...el, transform };
}

/** Translates el's data by (dx,dy) in world space, for every element data shape. */
export function translateElement(el: ElementData, dx: number, dy: number): ElementData {
  const d = el.data as Record<string, any>;
  let moved: ElementData;

  if (el.elementType === "line" || el.elementType === "arrow") {
    moved = {
      ...el,
      data: { ...d, x1: (d.x1 ?? 0) + dx, y1: (d.y1 ?? 0) + dy, x2: (d.x2 ?? 0) + dx, y2: (d.y2 ?? 0) + dy },
    };
  } else if (el.elementType === "freehand") {
    const points: number[][] = (d.points ?? []).map(([x, y, pressure]: number[]) => [x + dx, y + dy, pressure]);
    moved = { ...el, data: { ...d, points } };
  } else {
    // Box types (rectangle/ellipse/sticky/text/image) and any unrecognized type.
    moved = { ...el, data: { ...d, x: (d.x ?? 0) + dx, y: (d.y ?? 0) + dy } };
  }

  // A move re-centers a rotated element's pivot. Re-derive the transform at the
  // element's *new* center (same angle) so an already-rotated shape keeps its
  // orientation instead of orbiting around its old position.
  if (!isIdentity(el.transform)) {
    return rotateElementTransform(moved, currentRotation(el));
  }
  return moved;
}

const HANDLE_EDGES: Record<string, { n?: boolean; s?: boolean; e?: boolean; w?: boolean }> = {
  nw: { n: true, w: true },
  n: { n: true },
  ne: { n: true, e: true },
  e: { e: true },
  se: { s: true, e: true },
  s: { s: true },
  sw: { s: true, w: true },
  w: { w: true },
};

/**
 * Resizes a box-type element (rectangle/ellipse/sticky/text/image) from handleId +
 * world delta. Normalizes so width/height never go negative — dragging a corner
 * past its opposite corner flips the box instead of collapsing/inverting it.
 */
function resizeBox(el: ElementData, handleId: string, dx: number, dy: number): ElementData {
  const d = el.data as Record<string, any>;
  const edges = HANDLE_EDGES[handleId];
  const left0 = d.x ?? 0;
  const top0 = d.y ?? 0;
  const right0 = left0 + (d.width ?? 0);
  const bottom0 = top0 + (d.height ?? 0);

  let left = left0;
  let top = top0;
  let right = right0;
  let bottom = bottom0;
  if (edges?.n) top += dy;
  if (edges?.s) bottom += dy;
  if (edges?.w) left += dx;
  if (edges?.e) right += dx;

  const x = Math.min(left, right);
  const y = Math.min(top, bottom);
  const width = Math.abs(right - left);
  const height = Math.abs(bottom - top);
  return { ...el, data: { ...d, x, y, width, height } };
}

/** Resizes a line/arrow by moving whichever endpoint is nearest the dragged handle. */
function resizeLine(el: ElementData, handleId: string, dx: number, dy: number): ElementData {
  const d = el.data as Record<string, any>;
  const x1 = d.x1 ?? 0;
  const y1 = d.y1 ?? 0;
  const x2 = d.x2 ?? 0;
  const y2 = d.y2 ?? 0;

  const handle = resizeHandles(elementBounds(el)).find((h) => h.id === handleId);
  const hx = handle?.x ?? x1;
  const hy = handle?.y ?? y1;
  const dist1 = Math.hypot(x1 - hx, y1 - hy);
  const dist2 = Math.hypot(x2 - hx, y2 - hy);

  if (dist1 <= dist2) {
    return { ...el, data: { ...d, x1: x1 + dx, y1: y1 + dy, x2, y2 } };
  }
  return { ...el, data: { ...d, x1, y1, x2: x2 + dx, y2: y2 + dy } };
}

/**
 * Recomputes el's box from a dragged resize handle (nw/n/ne/e/se/s/sw/w) and a
 * cumulative world-space (dx,dy) delta from the drag's start point. Box types
 * update x/y/width/height; line/arrow move whichever endpoint is nearest the
 * dragged handle. Unsupported shapes (freehand) pass through unchanged.
 * If el is rotated, the transform is re-derived at the resized box's new center
 * so the rotation pivot follows the box instead of staying at the old center.
 */
export function resizeElement(el: ElementData, handleId: string, dx: number, dy: number): ElementData {
  let resized: ElementData;
  if (BOX_TYPES.has(el.elementType)) {
    resized = resizeBox(el, handleId, dx, dy);
  } else if (el.elementType === "line" || el.elementType === "arrow") {
    resized = resizeLine(el, handleId, dx, dy);
  } else {
    return el;
  }

  if (!isIdentity(el.transform)) {
    return rotateElementTransform(resized, currentRotation(el));
  }
  return resized;
}

/**
 * Clones el as a brand-new element (fresh id + seed, makeElement-style) offset by
 * (dx,dy) in world space. Used by paste and duplicate.
 */
export function cloneElementOffset(el: ElementData, dx = 16, dy = 16): ElementData {
  const moved = translateElement(el, dx, dy);
  return { ...moved, id: crypto.randomUUID(), seed: Math.floor(Math.random() * 2 ** 31) };
}

/**
 * The element's axis-aligned bounding box in WORLD space — el's local bounds'
 * 4 corners, each mapped through el.transform, then re-boxed. For an unrotated
 * element this equals `elementBounds(el)`; for a rotated one it's the actual
 * on-screen footprint, which is what marquee selection needs to test against
 * (elementBounds/hitInMarquee alone operate in local, pre-rotation space).
 */
export function elementWorldBounds(el: ElementData): Bounds {
  const b = elementBounds(el);
  const corners = [
    { x: b.x, y: b.y },
    { x: b.x + b.width, y: b.y },
    { x: b.x + b.width, y: b.y + b.height },
    { x: b.x, y: b.y + b.height },
  ].map((p) => applyTransform(el.transform, p));
  const xs = corners.map((p) => p.x);
  const ys = corners.map((p) => p.y);
  const minX = Math.min(...xs);
  const minY = Math.min(...ys);
  const maxX = Math.max(...xs);
  const maxY = Math.max(...ys);
  return { x: minX, y: minY, width: maxX - minX, height: maxY - minY };
}

/**
 * True if el's WORLD-space footprint is fully contained within rect (the
 * marquee selection box) — the rotation-aware counterpart to hitTest.ts's
 * `hitInMarquee`, which tests el's local (pre-rotation) bounds instead.
 */
export function elementInMarquee(el: ElementData, rect: Bounds): boolean {
  const b = elementWorldBounds(el);
  return (
    b.x >= rect.x &&
    b.y >= rect.y &&
    b.x + b.width <= rect.x + rect.width &&
    b.y + b.height <= rect.y + rect.height
  );
}
