import type { ElementData } from "../stores/model";

export interface Bounds {
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface Point {
  x: number;
  y: number;
}

const BOX_TYPES = new Set(["rectangle", "ellipse", "sticky", "image", "text"]);

/** Squared distance from point p to segment (a,b). Avoids a sqrt for comparisons. */
export function distToSegmentSq(p: Point, a: Point, b: Point): number {
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const lenSq = dx * dx + dy * dy;
  if (lenSq === 0) {
    const ddx = p.x - a.x;
    const ddy = p.y - a.y;
    return ddx * ddx + ddy * ddy;
  }
  let t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
  t = Math.max(0, Math.min(1, t));
  const cx = a.x + t * dx;
  const cy = a.y + t * dy;
  const ddx = p.x - cx;
  const ddy = p.y - cy;
  return ddx * ddx + ddy * ddy;
}

/** Distance from point p to segment (a,b). */
export function distToSegment(p: Point, a: Point, b: Point): number {
  return Math.sqrt(distToSegmentSq(p, a, b));
}

/** Bounding box of an element, in element-local (untransformed) coordinates. */
export function elementBounds(el: ElementData): Bounds {
  const d = el.data as Record<string, any>;

  if (BOX_TYPES.has(el.elementType)) {
    return {
      x: d.x ?? 0,
      y: d.y ?? 0,
      width: d.width ?? 0,
      height: d.height ?? 0,
    };
  }

  if (el.elementType === "line" || el.elementType === "arrow") {
    const x1 = d.x1 ?? 0;
    const y1 = d.y1 ?? 0;
    const x2 = d.x2 ?? 0;
    const y2 = d.y2 ?? 0;
    const minX = Math.min(x1, x2);
    const minY = Math.min(y1, y2);
    return {
      x: minX,
      y: minY,
      width: Math.max(x1, x2) - minX,
      height: Math.max(y1, y2) - minY,
    };
  }

  if (el.elementType === "freehand") {
    const points: number[][] = d.points ?? [];
    if (points.length === 0) return { x: 0, y: 0, width: 0, height: 0 };
    let minX = Infinity;
    let minY = Infinity;
    let maxX = -Infinity;
    let maxY = -Infinity;
    for (const [px, py] of points) {
      if (px < minX) minX = px;
      if (py < minY) minY = py;
      if (px > maxX) maxX = px;
      if (py > maxY) maxY = py;
    }
    return { x: minX, y: minY, width: maxX - minX, height: maxY - minY };
  }

  // Fallback for any unrecognized element type.
  return { x: d.x ?? 0, y: d.y ?? 0, width: d.width ?? 0, height: d.height ?? 0 };
}

/**
 * The two points of an arrowhead "V" at (x2,y2), pointing back along the
 * line from (x1,y1) to (x2,y2).
 */
export function arrowHead(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
  size: number
): { p1: [number, number]; p2: [number, number] } {
  const angle = Math.atan2(y2 - y1, x2 - x1);
  const spread = Math.PI / 7;
  return {
    p1: [x2 - size * Math.cos(angle - spread), y2 - size * Math.sin(angle - spread)],
    p2: [x2 - size * Math.cos(angle + spread), y2 - size * Math.sin(angle + spread)],
  };
}
