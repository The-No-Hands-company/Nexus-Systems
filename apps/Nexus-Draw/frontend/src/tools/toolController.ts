import type { ElementData } from "../stores/model";
import { makeElement } from "../stores/model";
import { useEditorStore } from "../stores/useEditorStore";
import { hitElement } from "../render/hitTest";

export interface Vec2 { x: number; y: number }

export type ToolName = "rectangle" | "ellipse" | "line" | "arrow" | "sticky" | "pen" | "text" | "eraser";

interface ToolState {
  tool: ToolName | "";
  active: boolean;
  start: Vec2;
  current: Vec2;
  points: Vec2[];
  draft: ElementData | null;
}

const state: ToolState = { tool: "", active: false, start: { x: 0, y: 0 }, current: { x: 0, y: 0 }, points: [], draft: null };
const GRID = 10;

function snapPoint(p: Vec2): Vec2 {
  const snap = useEditorStore.getState().board?.gridSnap ?? false;
  if (!snap) return p;
  return { x: Math.round(p.x / GRID) * GRID, y: Math.round(p.y / GRID) * GRID };
}

export function beginTool(tool: ToolName, worldPt: Vec2): void {
  const store = useEditorStore.getState();
  const p = snapPoint(worldPt);

  if (tool === "eraser") {
    const hit = [...store.elements].reverse().find((el) => hitElement(el, worldPt, 6));
    if (hit) store.removeElement(hit.id);
    return;
  }

  if (tool === "text") {
    const el = makeElement("text", { x: p.x, y: p.y, text: "" });
    store.addElement(el);
    store.setTextEditingId(el.id);
    return;
  }

  state.tool = tool;
  state.active = true;
  state.start = p;
  state.current = p;
  state.points = [p];
  state.draft = null;
}

export function updateTool(worldPt: Vec2): void {
  if (!state.active) return;
  const p = snapPoint(worldPt);
  state.current = p;

  switch (state.tool) {
    case "rectangle":
    case "ellipse":
    case "sticky": {
      const x = Math.min(state.start.x, p.x);
      const y = Math.min(state.start.y, p.y);
      state.draft = makeElement(state.tool, { x, y, width: Math.abs(p.x - state.start.x), height: Math.abs(p.y - state.start.y) });
      break;
    }
    case "line":
    case "arrow":
      state.draft = makeElement(state.tool, { x1: state.start.x, y1: state.start.y, x2: p.x, y2: p.y });
      break;
    case "pen": {
      state.points.push(p);
      state.draft = makeElement("freehand", { points: state.points.map((pt) => ({ x: pt.x, y: pt.y })) });
      break;
    }
    default:
      break;
  }
}

export function endTool(): void {
  if (!state.active) return;
  const store = useEditorStore.getState();
  if (state.draft) store.addElement(state.draft);
  reset();
}

export function reset(): void {
  state.tool = "";
  state.active = false;
  state.points = [];
  state.draft = null;
}

export function getPreview(): ElementData | null {
  return state.draft;
}

export function isDrawing(): boolean {
  return state.active;
}