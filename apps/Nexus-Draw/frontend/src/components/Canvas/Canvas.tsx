import { useEffect, useRef, useCallback } from "react";
import rough from "roughjs";
import { useEditorStore } from "../../stores/useEditorStore";
import { makeElement } from "../../stores/model";
import { resolveStyleMode } from "../../stores/model";
import { renderElement } from "../../render/renderElement";
import { elementBounds, resizeHandles } from "../../render/geometry";
import { hitElement } from "../../render/hitTest";

const GRID_SIZE = 40;

export default function Canvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const rcRef = useRef<ReturnType<typeof rough.canvas> | null>(null);
  const dragging = useRef(false);
  const lastMouse = useRef({ x: 0, y: 0 });
  const drawStart = useRef({ x: 0, y: 0 });
  const spacePanning = useRef(false);

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
  const selectedElementIds = useEditorStore((s) => s.selectedElementIds);

  const screenToWorld = useCallback((cx: number, cy: number) => {
    return { x: cx / zoom - pan.x, y: cy / zoom - pan.y };
  }, [pan, zoom]);

  const drawScene = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;
    const rc = rcRef.current;
    if (!rc) return;
    const dpr = window.devicePixelRatio || 1;

    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.fillStyle = "#1a1a2e";
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    ctx.setTransform(dpr * zoom, 0, 0, dpr * zoom, pan.x * dpr * zoom, pan.y * dpr * zoom);

    ctx.strokeStyle = "rgba(255,255,255,0.06)";
    ctx.lineWidth = 1 / zoom;
    const vw = canvas.width / dpr / zoom;
    const vh = canvas.height / dpr / zoom;
    ctx.beginPath();
    for (let x = ((-pan.x % GRID_SIZE) + GRID_SIZE) % GRID_SIZE; x < vw; x += GRID_SIZE) {
      ctx.moveTo(x, 0);
      ctx.lineTo(x, vh);
    }
    for (let y = ((-pan.y % GRID_SIZE) + GRID_SIZE) % GRID_SIZE; y < vh; y += GRID_SIZE) {
      ctx.moveTo(0, y);
      ctx.lineTo(vw, y);
    }
    ctx.stroke();

    const sorted = [...elements].sort((a, b) => a.order - b.order);
    const board = useEditorStore.getState().board;
    for (const el of sorted) renderElement(ctx, rc, el, resolveStyleMode(el, board?.defaultStyleMode ?? "clean"));

    for (const id of selectedElementIds) {
      const el = elements.find((e) => e.id === id);
      if (!el) continue;
      const b = elementBounds(el);
      ctx.strokeStyle = "#3b82f6";
      ctx.lineWidth = 2 / zoom;
      ctx.strokeRect(b.x, b.y, b.width, b.height);
      ctx.fillStyle = "#3b82f6";
      for (const h of resizeHandles(b)) {
        ctx.beginPath();
        ctx.arc(h.x, h.y, 4 / zoom, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }, [elements, selectedElementIds, pan, zoom]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    rcRef.current = rough.canvas(canvas);

    const dpr = window.devicePixelRatio || 1;
    canvas.width = canvas.clientWidth * dpr;
    canvas.height = canvas.clientHeight * dpr;

    const resize = () => {
      const d = window.devicePixelRatio || 1;
      canvas.width = canvas.clientWidth * d;
      canvas.height = canvas.clientHeight * d;
      drawScene();
    };
    resize();
    window.addEventListener("resize", resize);
    return () => window.removeEventListener("resize", resize);
  }, [drawScene]);

  useEffect(() => {
    drawScene();
  }, [drawScene]);

  const handleWheel = useCallback((e: React.WheelEvent) => {
    e.preventDefault();
    const delta = -e.deltaY * 0.001;
    setZoom(zoom * (1 + delta));
  }, [zoom, setZoom]);

  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    const rect = canvasRef.current!.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;
    const world = screenToWorld(mx, my);

    if (e.button === 1 || (e.button === 0 && activeTool === "hand")) {
      dragging.current = true;
      lastMouse.current = { x: e.clientX, y: e.clientY };
      return;
    }

    if (e.button === 0) {
      if (["rectangle", "ellipse", "line", "arrow"].includes(activeTool)) {
        dragging.current = true;
        drawStart.current = { x: world.x, y: world.y };
        lastMouse.current = { x: world.x, y: world.y };
      } else if (activeTool === "select") {
        const hit = [...elements].reverse().find((el) => hitElement(el, world, 4));
        if (hit) {
          selectElement(hit.id, e.metaKey || e.ctrlKey);
        } else {
          deselectAll();
        }
      }
    }
  }, [activeTool, screenToWorld, elements, selectElement, deselectAll]);

  const handleMouseMove = useCallback((e: React.MouseEvent) => {
    if (!dragging.current) return;

    if (activeTool === "hand") {
      const dx = e.clientX - lastMouse.current.x;
      const dy = e.clientY - lastMouse.current.y;
      setPan({ x: pan.x + dx / zoom, y: pan.y + dy / zoom });
      lastMouse.current = { x: e.clientX, y: e.clientY };
      return;
    }

    const rect = canvasRef.current!.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;
    const world = screenToWorld(mx, my);

    if (["rectangle", "ellipse", "line", "arrow"].includes(activeTool)) {
      lastMouse.current = { x: world.x, y: world.y };
    }
  }, [activeTool, pan, zoom, setPan, screenToWorld, elements]);

  const handleMouseUp = useCallback((e: React.MouseEvent) => {
    if (!dragging.current) return;
    dragging.current = false;

    if (["rectangle", "ellipse", "line", "arrow"].includes(activeTool)) {
      const rect = canvasRef.current!.getBoundingClientRect();
      const mx = e.clientX - rect.left;
      const my = e.clientY - rect.top;
      const world = screenToWorld(mx, my);

      const sx = drawStart.current.x;
      const sy = drawStart.current.y;
      const ex = world.x;
      const ey = world.y;
      const x = Math.min(sx, ex);
      const y = Math.min(sy, ey);
      const w = Math.abs(ex - sx);
      const h = Math.abs(ey - sy);

      if (w > 2 || h > 2) {
        const type = activeTool as "rectangle" | "ellipse" | "line" | "arrow";
        const data = type === "line" || type === "arrow"
          ? { x1: sx, y1: sy, x2: ex, y2: ey }
          : { x, y, width: w, height: h };
        addElement(makeElement(type, data));
      }
    }
  }, [activeTool, screenToWorld, addElement, elements]);

  const handleKeyDown = useCallback((e: KeyboardEvent) => {
    if (e.key === " " && !e.repeat) { setActiveTool("hand"); spacePanning.current = true; e.preventDefault(); }
    if (e.key === "v") { setActiveTool("select"); }
    if (e.key === "p") { setActiveTool("pen"); }
    if (e.key === "r") { setActiveTool("rectangle"); }
    if (e.key === "e") { setActiveTool("ellipse"); }
    if (e.key === "l") { setActiveTool("line"); }
    if (e.key === "a") { setActiveTool("arrow"); }
    if (e.key === "t") { setActiveTool("text"); }
    if (e.key === "s") { setActiveTool("sticky"); }
    if (e.key === "u" && (e.metaKey || e.ctrlKey) && !e.shiftKey) { e.preventDefault(); useEditorStore.getState().undo(); }
    if (e.key === "z" && (e.metaKey || e.ctrlKey) && e.shiftKey) { e.preventDefault(); useEditorStore.getState().redo(); }
    if ((e.metaKey || e.ctrlKey) && e.key === "0") { setZoom(1); setPan({ x: 0, y: 0 }); }
  }, [setActiveTool, setZoom, setPan]);

  const handleKeyUp = useCallback((e: KeyboardEvent) => {
    if (e.key === " " && activeTool === "hand") {
      setActiveTool("select");
      spacePanning.current = false;
    }
  }, [activeTool, setActiveTool]);

  useEffect(() => {
    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("keyup", handleKeyUp);
    return () => { window.removeEventListener("keydown", handleKeyDown); window.removeEventListener("keyup", handleKeyUp); };
  }, [handleKeyDown, handleKeyUp]);

  return (
    <canvas
      ref={canvasRef}
      className="w-full h-full cursor-crosshair"
      style={{ background: "#1a1a2e" }}
      onWheel={handleWheel}
      onMouseDown={handleMouseDown}
      onMouseMove={handleMouseMove}
      onMouseUp={handleMouseUp}
      onMouseLeave={handleMouseUp}
    />
  );
}