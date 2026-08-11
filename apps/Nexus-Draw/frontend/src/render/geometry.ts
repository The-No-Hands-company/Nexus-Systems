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

export const BOX_TYPES = new Set(["rectangle", "ellipse", "sticky", "image", "text"]);

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

/** Bounding box of an element, in element-local (untransformed) coordinates. */
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
