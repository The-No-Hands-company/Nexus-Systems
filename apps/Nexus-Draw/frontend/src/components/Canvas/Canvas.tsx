import { useEffect, useRef, useCallback } from "react";
import rough from "roughjs";
import type { RoughCanvas } from "roughjs/bin/canvas";
import { useEditorStore, type Vec2 } from "../../stores/useEditorStore";
import { makeElement, resolveStyleMode, type ElementData, type ElementStyle, type StyleMode } from "../../stores/model";
import { renderElement } from "../../render/renderElement";
import { hitElement, elementBounds, resizeHandles } from "../../render/hitTest";

const GRID_SIZE = 40;
const GRID_COLOR = "rgba(255,255,255,0.06)";
const SELECTION_COLOR = "#3b82f6";
const HANDLE_SCREEN_SIZE = 8;
const SELECT_TOLERANCE_PX = 6;

type ShapeTool = "rectangle" | "ellipse" | "line" | "arrow";
const SHAPE_TOOLS: ReadonlySet<string> = new Set<ShapeTool>(["rectangle", "ellipse", "line", "arrow"]);

function isShapeTool(tool: string): tool is ShapeTool {
  return SHAPE_TOOLS.has(tool);
}

interface DraftShape {
  tool: ShapeTool;
  startX: number;
  startY: number;
  endX: number;
  endY: number;
  seed: number;
}

function shapeData(tool: ShapeTool, x1: number, y1: number, x2: number, y2: number): Record<string, number> {
  if (tool === "line" || tool === "arrow") {
    return { x1, y1, x2, y2 };
  }
  return {
    x: Math.min(x1, x2),
    y: Math.min(y1, y2),
    width: Math.abs(x2 - x1),
    height: Math.abs(y2 - y1),
  };
}

function draftToElement(draft: DraftShape, boardDefault: StyleMode): ElementData {
  const style: ElementStyle = {
    stroke: "#60a5fa",
    fill: "none",
    strokeWidth: 2,
    strokeStyle: "solid",
    opacity: 0.85,
    radius: 8,
    fontFamily: "ui-sans-serif, system-ui",
    fontSize: 20,
    textAlign: "left",
    styleMode: boardDefault,
  };
  return {
    id: "__draft__",
    elementType: draft.tool,
    data: shapeData(draft.tool, draft.startX, draft.startY, draft.endX, draft.endY),
    style,
    transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
    order: Number.MAX_SAFE_INTEGER,
    seed: draft.seed,
  };
}

/** Draws the world-space grid visible within the current pan/zoom window. */
function drawGrid(ctx: CanvasRenderingContext2D, pan: Vec2, zoom: number, cssWidth: number, cssHeight: number): void {
  const left = -pan.x;
  const top = -pan.y;
  const right = left + cssWidth / zoom;
  const bottom = top + cssHeight / zoom;
  const startX = Math.floor(left / GRID_SIZE) * GRID_SIZE;
  const startY = Math.floor(top / GRID_SIZE) * GRID_SIZE;

  ctx.save();
  ctx.strokeStyle = GRID_COLOR;
  ctx.lineWidth = 1 / zoom;
  ctx.beginPath();
  for (let x = startX; x <= right; x += GRID_SIZE) {
    ctx.moveTo(x, top);
    ctx.lineTo(x, bottom);
  }
  for (let y = startY; y <= bottom; y += GRID_SIZE) {
    ctx.moveTo(left, y);
    ctx.lineTo(right, y);
  }
  ctx.stroke();
  ctx.restore();
}

/** Draws bounds + resize/rotate handles for every selected element. */
function drawSelectionOverlay(
  ctx: CanvasRenderingContext2D,
  elements: ElementData[],
  selectedIds: Set<string>,
  zoom: number
): void {
  if (selectedIds.size === 0) return;
  const handleSize = HANDLE_SCREEN_SIZE / zoom;

  ctx.save();
  for (const el of elements) {
    if (!selectedIds.has(el.id)) continue;
    const b = elementBounds(el);

    ctx.strokeStyle = SELECTION_COLOR;
    ctx.lineWidth = 1 / zoom;
    ctx.setLineDash([4 / zoom, 3 / zoom]);
    ctx.strokeRect(b.x, b.y, b.width, b.height);

    ctx.setLineDash([]);
    ctx.fillStyle = "#ffffff";
    for (const handle of resizeHandles(b)) {
      if (handle.id === "rotate") {
        ctx.beginPath();
        ctx.arc(handle.x, handle.y, handleSize / 2, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();
        continue;
      }
      ctx.fillRect(handle.x - handleSize / 2, handle.y - handleSize / 2, handleSize, handleSize);
      ctx.strokeRect(handle.x - handleSize / 2, handle.y - handleSize / 2, handleSize, handleSize);
    }
  }
  ctx.restore();
}

export default function Canvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const roughRef = useRef<RoughCanvas | null>(null);
  const ctxRef = useRef<CanvasRenderingContext2D | null>(null);
  const rafRef = useRef<number>(0);
  const panDragging = useRef(false);
  const lastMouse = useRef({ x: 0, y: 0 });
  const draftRef = useRef<DraftShape | null>(null);

  const pan = useEditorStore((s) => s.pan);
  const zoom = useEditorStore((s) => s.zoom);
  const setPan = useEditorStore((s) => s.setPan);
  const setZoom = useEditorStore((s) => s.setZoom);
  const elements = useEditorStore((s) => s.elements);
  const activeTool = useEditorStore((s) => s.activeTool);
  const selectElement = useEditorStore((s) => s.selectElement);
  const deselectAll = useEditorStore((s) => s.deselectAll);
  const setActiveTool = useEditorStore((s) => s.setActiveTool);
  const addElement = useEditorStore((s) => s.addElement);

  const screenToWorld = useCallback(
    (cx: number, cy: number) => ({ x: cx / zoom - pan.x, y: cy / zoom - pan.y }),
    [pan, zoom]
  );

  // Canvas setup + DPR resize handling + the continuous draw loop.
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;
    ctxRef.current = ctx;
    roughRef.current = rough.canvas(canvas);

    const resize = () => {
      const dpr = window.devicePixelRatio || 1;
      canvas.width = Math.max(1, Math.floor(canvas.clientWidth * dpr));
      canvas.height = Math.max(1, Math.floor(canvas.clientHeight * dpr));
    };
    resize();
    window.addEventListener("resize", resize);
    const observer = new ResizeObserver(resize);
    observer.observe(canvas);

    const draw = () => {
      const cv = canvasRef.current;
      const ctx2 = ctxRef.current;
      const rc = roughRef.current;
      if (cv && ctx2 && rc) {
        const dpr = window.devicePixelRatio || 1;
        const store = useEditorStore.getState();
        const curPan = store.pan;
        const curZoom = store.zoom;
        const boardDefault = store.getDefaultStyleMode();

        ctx2.setTransform(1, 0, 0, 1, 0, 0);
        ctx2.clearRect(0, 0, cv.width, cv.height);
        ctx2.setTransform(dpr * curZoom, 0, 0, dpr * curZoom, curPan.x * dpr * curZoom, curPan.y * dpr * curZoom);

        drawGrid(ctx2, curPan, curZoom, cv.clientWidth, cv.clientHeight);

        const sorted = [...store.elements].sort((a, b) => a.order - b.order);
        for (const el of sorted) {
          renderElement(ctx2, rc, el, resolveStyleMode(el, boardDefault));
        }

        const draft = draftRef.current;
        if (draft) {
          const draftEl = draftToElement(draft, boardDefault);
          renderElement(ctx2, rc, draftEl, resolveStyleMode(draftEl, boardDefault));
        }

        drawSelectionOverlay(ctx2, store.elements, store.selectedElementIds, curZoom);
      }
      rafRef.current = requestAnimationFrame(draw);
    };
    rafRef.current = requestAnimationFrame(draw);

    return () => {
      window.removeEventListener("resize", resize);
      observer.disconnect();
      cancelAnimationFrame(rafRef.current);
    };
  }, []);

  const handleWheel = useCallback(
    (e: React.WheelEvent) => {
      e.preventDefault();
      const delta = -e.deltaY * 0.001;
      setZoom(zoom * (1 + delta));
    },
    [zoom, setZoom]
  );

  const handleMouseDown = useCallback(
    (e: React.MouseEvent) => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const rect = canvas.getBoundingClientRect();
      const world = screenToWorld(e.clientX - rect.left, e.clientY - rect.top);

      if (e.button === 1 || (e.button === 0 && activeTool === "hand")) {
        panDragging.current = true;
        lastMouse.current = { x: e.clientX, y: e.clientY };
        return;
      }

      if (e.button !== 0) return;

      if (isShapeTool(activeTool)) {
        draftRef.current = {
          tool: activeTool,
          startX: world.x,
          startY: world.y,
          endX: world.x,
          endY: world.y,
          seed: Math.floor(Math.random() * 2 ** 31),
        };
        return;
      }

      if (activeTool === "select") {
        const tol = SELECT_TOLERANCE_PX / zoom;
        const hit = [...elements].reverse().find((el) => hitElement(el, world, tol));
        if (hit) {
          selectElement(hit.id, e.metaKey || e.ctrlKey);
        } else {
          deselectAll();
        }
      }
    },
    [activeTool, screenToWorld, elements, zoom, selectElement, deselectAll]
  );

  const handleMouseMove = useCallback(
    (e: React.MouseEvent) => {
      if (panDragging.current) {
        const dx = e.clientX - lastMouse.current.x;
        const dy = e.clientY - lastMouse.current.y;
        setPan({ x: pan.x + dx / zoom, y: pan.y + dy / zoom });
        lastMouse.current = { x: e.clientX, y: e.clientY };
        return;
      }

      const draft = draftRef.current;
      if (!draft) return;
      const canvas = canvasRef.current;
      if (!canvas) return;
      const rect = canvas.getBoundingClientRect();
      const world = screenToWorld(e.clientX - rect.left, e.clientY - rect.top);
      draftRef.current = { ...draft, endX: world.x, endY: world.y };
    },
    [pan, zoom, setPan, screenToWorld]
  );

  const handleMouseUp = useCallback(() => {
    if (panDragging.current) {
      panDragging.current = false;
      return;
    }

    const draft = draftRef.current;
    draftRef.current = null;
    if (!draft) return;

    const dx = draft.endX - draft.startX;
    const dy = draft.endY - draft.startY;
    if (Math.abs(dx) < 2 && Math.abs(dy) < 2) return;

    const data = shapeData(draft.tool, draft.startX, draft.startY, draft.endX, draft.endY);
    const newEl = makeElement(draft.tool, data);
    newEl.order = useEditorStore.getState().elements.length;
    addElement(newEl);
  }, [addElement]);

  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      if (e.key === " " && !e.repeat) {
        setActiveTool("hand");
        e.preventDefault();
      }
      if (e.key === "v") setActiveTool("select");
      if (e.key === "p") setActiveTool("pen");
      if (e.key === "r") setActiveTool("rectangle");
      if (e.key === "e") setActiveTool("ellipse");
      if (e.key === "l") setActiveTool("line");
      if (e.key === "a") setActiveTool("arrow");
      if (e.key === "t") setActiveTool("text");
      if (e.key === "s") setActiveTool("sticky");
      if (e.key === "u" && (e.metaKey || e.ctrlKey) && !e.shiftKey) {
        e.preventDefault();
        useEditorStore.getState().undo();
      }
      if (e.key === "z" && (e.metaKey || e.ctrlKey) && e.shiftKey) {
        e.preventDefault();
        useEditorStore.getState().redo();
      }
      if ((e.metaKey || e.ctrlKey) && e.key === "0") {
        setZoom(1);
        setPan({ x: 0, y: 0 });
      }
    },
    [setActiveTool, setZoom, setPan]
  );

  const handleKeyUp = useCallback(
    (e: KeyboardEvent) => {
      if (e.key === " " && activeTool === "hand") {
        setActiveTool("select");
      }
    },
    [activeTool, setActiveTool]
  );

  useEffect(() => {
    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("keyup", handleKeyUp);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
      window.removeEventListener("keyup", handleKeyUp);
    };
  }, [handleKeyDown, handleKeyUp]);

  return (
    <canvas
      ref={canvasRef}
      className={`w-full h-full ${activeTool === "hand" ? "cursor-grab" : "cursor-crosshair"}`}
      style={{ background: "#1a1a2e" }}
      onWheel={handleWheel}
      onMouseDown={handleMouseDown}
      onMouseMove={handleMouseMove}
      onMouseUp={handleMouseUp}
      onMouseLeave={handleMouseUp}
    />
  );
}
