import type { ElementData } from "../stores/model";

export interface Rect { x: number; y: number; width: number; height: number }
export interface Point { x: number; y: number }

export function elementBounds(el: ElementData): Rect {
  switch (el.elementType) {
    case "line":
    case "arrow": {
      const d = el.data as { x1: number; y1: number; x2: number; y2: number };
      const x = Math.min(d.x1, d.x2);
      const y = Math.min(d.y1, d.y2);
      return { x, y, width: Math.abs(d.x2 - d.x1), height: Math.abs(d.y2 - d.y1) };
    }
    case "freehand": {
      const pts = el.data.points as Point[];
      const xs = pts.map((p) => p.x);
      const ys = pts.map((p) => p.y);
      const minX = Math.min(...xs);
      const maxX = Math.max(...xs);
      const minY = Math.min(...ys);
      const maxY = Math.max(...ys);
      return { x: minX, y: minY, width: maxX - minX, height: maxY - minY };
    }
    default: {
      const d = el.data as { x: number; y: number; width: number; height: number };
      return { x: d.x ?? 0, y: d.y ?? 0, width: d.width ?? 1, height: d.height ?? 1 };
    }
  }
}

export function distanceToSegment(p: Point, a: Point, b: Point): number {
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const lenSq = dx * dx + dy * dy;
  if (lenSq === 0) return Math.hypot(p.x - a.x, p.y - a.y);
  let t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
  t = Math.max(0, Math.min(1, t));
  return Math.hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

export function pointInRect(p: Point, r: Rect, tol = 0): boolean {
  return p.x >= r.x - tol && p.x <= r.x + r.width + tol &&
         p.y >= r.y - tol && p.y <= r.y + r.height + tol;
}

export function resizeHandles(b: Rect): { id: string; x: number; y: number }[] {
  const { x, y, width: w, height: h } = b;
  const mw = w / 2;
  const mh = h / 2;
  return [
    { id: "nw", x, y },
    { id: "n", x: x + mw, y },
    { id: "ne", x: x + w, y },
    { id: "e", x: x + w, y: y + mh },
    { id: "se", x: x + w, y: y + h },
    { id: "s", x: x + mw, y: y + h },
    { id: "sw", x, y: y + h },
    { id: "w", x, y: y + mh },
    { id: "rotate", x: x + mw, y: y - 32 },
  ];
}

export function arrowHead(x1: number, y1: number, x2: number, y2: number, size: number): { p1: [number, number]; p2: [number, number] } {
  const angle = Math.atan2(y2 - y1, x2 - x1);
  const a = size;
  const b = size * 0.5;
  return {
    p1: [x2 - a * Math.cos(angle - b), y2 - a * Math.sin(angle - b)],
    p2: [x2 - a * Math.cos(angle + b), y2 - a * Math.sin(angle + b)],
  };
}