import type { ElementData } from "../stores/model";
import { elementBounds, resizeHandles, type Point, type Rect } from "../render/geometry";
import { hitInMarquee } from "../render/hitTest";

export type SelMode = "move" | "resize" | "rotate" | "marquee" | "idle";

interface SelState {
  mode: SelMode;
  active: boolean;
  start: Point;
  current: Point;
  handleId: string | null;
  target: ElementData | null;
  center: Point | null;
  initialAngle: number;
  selected: ElementData[];
  orig: Map<string, Record<string, unknown>>;
  marquee: Rect | null;
}

const state: SelState = {
  mode: "idle",
  active: false,
  start: { x: 0, y: 0 },
  current: { x: 0, y: 0 },
  handleId: null,
  target: null,
  center: null,
  initialAngle: 0,
  selected: [],
  orig: new Map(),
  marquee: null,
};

function cloneData(data: Record<string, unknown>): Record<string, unknown> {
  return JSON.parse(JSON.stringify(data));
}

function transformForRotation(cx: number, cy: number, ang: number): { a: number; b: number; c: number; d: number; e: number; f: number } {
  const c = Math.cos(ang);
  const s = Math.sin(ang);
  return { a: c, b: s, c: -s, d: c, e: cx - c * cx + s * cy, f: cy - s * cx - c * cy };
}

function rotationAngle(el: ElementData): number {
  return Math.atan2(el.transform.b, el.transform.a);
}

export function isOnHandle(el: ElementData, p: Point, tol: number): string | null {
  const b = elementBounds(el);
  for (const h of resizeHandles(b)) {
    if (Math.hypot(h.x - p.x, h.y - p.y) <= Math.max(tol, 8)) return h.id;
  }
  return null;
}

export function topmostHit(elements: ElementData[], p: Point, tol: number): ElementData | null {
  return [...elements].reverse().find((el) => {
    if (el.data.locked || el.data.hidden) return false;
    const b = elementBounds(el);
    return p.x >= b.x - tol && p.x <= b.x + b.width + tol && p.y >= b.y - tol && p.y <= b.y + b.height + tol;
  }) ?? null;
}

export function beginMove(selected: ElementData[], start: Point): void {
  state.mode = "move";
  state.active = true;
  state.start = start;
  state.current = start;
  state.selected = selected.map((el) => ({ ...el, data: cloneData(el.data) }));
  state.orig = new Map(selected.map((el) => [el.id, cloneData(el.data)]));
}

export function beginResize(target: ElementData, handleId: string, start: Point): void {
  state.mode = "resize";
  state.active = true;
  state.handleId = handleId;
  state.start = start;
  state.current = start;
  state.target = { ...target, data: cloneData(target.data) };
}

export function beginRotate(target: ElementData, start: Point): void {
  state.mode = "rotate";
  state.active = true;
  state.start = start;
  state.current = start;
  state.target = { ...target, data: cloneData(target.data) };
  const b = elementBounds(target);
  state.center = { x: b.x + b.width / 2, y: b.y + b.height / 2 };
  state.initialAngle = rotationAngle(target);
}

export function beginMarquee(start: Point): void {
  state.mode = "marquee";
  state.active = true;
  state.start = start;
  state.current = start;
  state.marquee = { x: start.x, y: start.y, width: 0, height: 0 };
}

function shiftData(data: Record<string, unknown>, dx: number, dy: number): Record<string, unknown> {
  const d = cloneData(data);
  if (typeof d.x === "number" && typeof d.y === "number") { d.x = (d.x as number) + dx; d.y = (d.y as number) + dy; }
  if (typeof d.x1 === "number" && typeof d.y1 === "number") { d.x1 = (d.x1 as number) + dx; d.y1 = (d.y1 as number) + dy; }
  if (typeof d.x2 === "number" && typeof d.y2 === "number") { d.x2 = (d.x2 as number) + dx; d.y2 = (d.y2 as number) + dy; }
  if (Array.isArray(d.points)) {
    d.points = (d.points as Point[]).map((pt) => ({ x: pt.x + dx, y: pt.y + dy }));
  }
  return d;
}

function resizeData(target: ElementData, startBounds: Rect, p: Point): Record<string, unknown> {
  const d = cloneData(target.data);
  if (target.elementType === "line" || target.elementType === "arrow") {
    const x1 = startBounds.x;
    const y1 = startBounds.y;
    const x2 = startBounds.x + startBounds.width;
    const y2 = startBounds.y + startBounds.height;
    const h = state.handleId ?? "";
    d.x1 = x1; d.y1 = y1; d.x2 = x2; d.y2 = y2;
    if (h.includes("e")) d.x2 = p.x;
    if (h.includes("w")) d.x1 = p.x;
    if (h.includes("s")) d.y2 = p.y;
    if (h.includes("n")) d.y1 = p.y;
    return d;
  }
  let { x, y, width, height } = startBounds;
  const h = state.handleId ?? "";
  switch (h) {
    case "nw": x = p.x; y = p.y; width = startBounds.x + startBounds.width - p.x; height = startBounds.y + startBounds.height - p.y; break;
    case "n": y = p.y; height = startBounds.y + startBounds.height - p.y; break;
    case "ne": y = p.y; width = p.x - startBounds.x; height = startBounds.y + startBounds.height - p.y; break;
    case "e": width = p.x - startBounds.x; break;
    case "se": width = p.x - startBounds.x; height = p.y - startBounds.y; break;
    case "s": height = p.y - startBounds.y; break;
    case "sw": x = p.x; width = startBounds.x + startBounds.width - p.x; height = p.y - startBounds.y; break;
    case "w": x = p.x; width = startBounds.x + startBounds.width - p.x; break;
    default: break;
  }
  d.x = x; d.y = y; d.width = Math.max(width, 1); d.height = Math.max(height, 1);
  return d;
}

export function updateSel(p: Point): void {
  if (!state.active) return;
  state.current = p;
  switch (state.mode) {
    case "move": {
      const dx = p.x - state.start.x;
      const dy = p.y - state.start.y;
      state.selected = state.selected.map((el) => ({
        ...el,
        data: shiftData(state.orig.get(el.id) ?? el.data, dx, dy),
      }));
      break;
    }
    case "resize": {
      if (!state.target) break;
      const b = elementBounds(state.target);
      state.target = { ...state.target, data: resizeData(state.target, b, p) };
      break;
    }
    case "rotate": {
      if (!state.target || !state.center) break;
      const a0 = Math.atan2(state.start.y - state.center.y, state.start.x - state.center.x);
      const a1 = Math.atan2(p.y - state.center.y, p.x - state.center.x);
      state.target = {
        ...state.target,
        transform: transformForRotation(state.center.x, state.center.y, state.initialAngle + (a1 - a0)),
      };
      break;
    }
    case "marquee": {
      state.marquee = {
        x: Math.min(state.start.x, p.x),
        y: Math.min(state.start.y, p.y),
        width: Math.abs(p.x - state.start.x),
        height: Math.abs(p.y - state.start.y),
      };
      break;
    }
    default:
      break;
  }
}

export interface SelResult {
  updates: { id: string; data?: Record<string, unknown>; transform?: { a: number; b: number; c: number; d: number; e: number; f: number } }[];
  marqueeSelect: string[];
}

export function endSel(elements: ElementData[]): SelResult {
  const res: SelResult = { updates: [], marqueeSelect: [] };
  if (state.mode === "move") {
    for (const el of state.selected) {
      const orig = state.orig.get(el.id);
      if (orig && JSON.stringify(orig) !== JSON.stringify(el.data)) {
        res.updates.push({ id: el.id, data: el.data });
      }
    }
  } else if (state.mode === "resize" && state.target) {
    res.updates.push({ id: state.target.id, data: state.target.data });
  } else if (state.mode === "rotate" && state.target) {
    res.updates.push({ id: state.target.id, transform: state.target.transform });
  } else if (state.mode === "marquee" && state.marquee) {
    res.marqueeSelect = elements.filter((el) => hitInMarquee(el, state.marquee!)).map((el) => el.id);
    res.marqueeSelect.sort((a, b) => a.localeCompare(b));
  }
  state.mode = "idle";
  state.active = false;
  state.selected = [];
  state.orig = new Map();
  state.target = null;
  state.marquee = null;
  state.handleId = null;
  return res;
}

export function cancelSel(): void {
  state.mode = "idle";
  state.active = false;
  state.selected = [];
  state.orig = new Map();
  state.target = null;
  state.marquee = null;
  state.handleId = null;
}

export function getSelPreview(): { type: "marquee" | "box"; rect: Rect; el?: ElementData; els?: ElementData[] } | null {
  if (state.mode === "marquee" && state.marquee) return { type: "marquee", rect: state.marquee };
  if (state.mode === "move" && state.selected.length > 0) {
    const els = state.selected;
    return { type: "box", rect: elementBounds(els[0]), el: els[0], els };
  }
  if (state.mode === "resize" && state.target) {
    return { type: "box", rect: elementBounds(state.target), el: state.target };
  }
  if (state.mode === "rotate" && state.target) {
    return { type: "box", rect: elementBounds(state.target), el: state.target };
  }
  return null;
}

export function isSelecting(): boolean {
  return state.active;
}

export function bringForward(elements: ElementData[], selectedIds: Set<string>): string[] {
  const arr = [...elements];
  let top = -1;
  arr.forEach((e, i) => { if (selectedIds.has(e.id)) top = Math.max(top, i); });
  if (top >= 0 && top + 1 < arr.length && !selectedIds.has(arr[top + 1].id)) {
    const tmp = arr[top]; arr[top] = arr[top + 1]; arr[top + 1] = tmp;
  }
  return arr.map((e) => e.id);
}

export function sendBackward(elements: ElementData[], selectedIds: Set<string>): string[] {
  const arr = [...elements];
  let bottom = -1;
  arr.forEach((e, i) => { if (selectedIds.has(e.id)) bottom = bottom === -1 ? i : Math.min(bottom, i); });
  if (bottom > 0 && !selectedIds.has(arr[bottom - 1].id)) {
    const tmp = arr[bottom]; arr[bottom] = arr[bottom - 1]; arr[bottom - 1] = tmp;
  }
  return arr.map((e) => e.id);
}