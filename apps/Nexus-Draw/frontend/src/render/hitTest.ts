import type { ElementData } from "../stores/model";
import { elementBounds, distanceToSegment, pointInRect, type Rect, type Point } from "./geometry";

export function hitElement(el: ElementData, p: Point, tol: number): boolean {
  if (el.data.locked || el.data.hidden) return false;
  const b = elementBounds(el);
  switch (el.elementType) {
    case "line":
    case "arrow":
      return distanceToSegment(p, { x: el.data.x1, y: el.data.y1 }, { x: el.data.x2, y: el.data.y2 }) <= tol + (el.style.strokeWidth || 2);
    case "freehand": {
      const pts = el.data.points as Point[];
      for (let i = 0; i < pts.length - 1; i++) {
        if (distanceToSegment(p, pts[i], pts[i + 1]) <= tol + (el.style.strokeWidth || 2)) return true;
      }
      return false;
    }
    default:
      return pointInRect(p, b, tol);
  }
}

export function hitInMarquee(el: ElementData, rect: Rect): boolean {
  if (el.data.locked || el.data.hidden) return false;
  const b = elementBounds(el);
  return b.x >= rect.x && b.y >= rect.y &&
         b.x + b.width <= rect.x + rect.width &&
         b.y + b.height <= rect.y + rect.height;
}

export { elementBounds };