import { useEffect, useRef, useCallback, useState } from "react";
import rough from "roughjs";
import { useEditorStore } from "../../stores/useEditorStore";
import { resolveStyleMode } from "../../stores/model";
import { renderElement } from "../../render/renderElement";
import { elementBounds, resizeHandles } from "../../render/geometry";
import { beginTool, updateTool, endTool, getPreview, isDrawing, type ToolName } from "../../tools/toolController";

const GRID_SIZE = 40;

function isDrawnTool(tool: string): tool is ToolName {
  return ["rectangle", "ellipse", "line", "arrow", "sticky", "pen", "text", "eraser"].includes(tool);
}

export default function Canvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const rcRef = useRef<ReturnType<typeof rough.canvas> | null>(null);
  const panning = useRef(false);
  const lastMouse = useRef({ x: 0, y: 0 });
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
  const selectedElementIds = useEditorStore((s) => s.selectedElementIds);
  const addElement = useEditorStore((s) => s.addElement);

  const screenToWorld = useCallback((cx: number, cy: number) => {
    return { x: cx / zoom - pan.x, y: cy / zoom - pan.y };
  }, [pan, zoom]);

  const worldToScreen = useCallback((wx: number, wy: number) => {
    return { x: (wx + pan.x) * zoom, y: (wy + pan.y) * zoom };
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

    const board = useEditorStore.getState().board;
    const sorted = [...elements].sort((a, b) => a.order - b.order);
    for (const el of sorted) renderElement(ctx, rc, el, resolveStyleMode(el, board?.defaultStyleMode ?? "clean"));

    const preview = getPreview();
    if (preview) renderElement(ctx, rc, preview, resolveStyleMode(preview, board?.defaultStyleMode ?? "clean"));
    void useEditorStore.getState();

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
  }, [elements, selectedElementIds, pan, zoom, addElement, setActiveTool]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    rcRef.current = rough.canvas(canvas);

    const resize = () => {
      const dpr = window.devicePixelRatio || 1;
      canvas.width = canvas.clientWidth * dpr;
      canvas.height = canvas.clientHeight * dpr;
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
    const zoomed = Math.max(0.1, Math.min(10, zoom * (1 - e.deltaY * 0.001)));
    setZoom(zoomed);
  }, [zoom, setZoom]);

  const handlePointerDown = useCallback((e: React.PointerEvent) => {
    if (e.button !== 0) {
      panning.current = true;
      lastMouse.current = { x: e.clientX, y: e.clientY };
      return;
    }
    const rect = canvasRef.current!.getBoundingClientRect();
    const world = screenToWorld(e.clientX - rect.left, e.clientY - rect.top);

    if (activeTool === "hand") {
      panning.current = true;
      lastMouse.current = { x: e.clientX, y: e.clientY };
      return;
    }

    if (activeTool === "select") {
      const hit = [...elements].reverse().find((el) => {
        const b = elementBounds(el);
        return world.x >= b.x && world.x <= b.x + b.width && world.y >= b.y && world.y <= b.y + b.height;
      });
      if (hit) {
        selectElement(hit.id, e.metaKey || e.ctrlKey);
      } else {
        deselectAll();
      }
      return;
    }

    if (isDrawnTool(activeTool)) {
      beginTool(activeTool, world);
      updateTool(world);
      drawScene();
    }
  }, [activeTool, screenToWorld, elements, selectElement, deselectAll, drawScene]);

  const handlePointerMove = useCallback((e: React.PointerEvent) => {
    if (panning.current) {
      const dx = e.clientX - lastMouse.current.x;
      const dy = e.clientY - lastMouse.current.y;
      setPan({ x: pan.x + dx / zoom, y: pan.y + dy / zoom });
      lastMouse.current = { x: e.clientX, y: e.clientY };
      return;
    }
    if (isDrawing()) {
      const rect = canvasRef.current!.getBoundingClientRect();
      const world = screenToWorld(e.clientX - rect.left, e.clientY - rect.top);
      updateTool(world);
      drawScene();
    }
  }, [pan, zoom, setPan, screenToWorld, drawScene]);

  const handlePointerUp = useCallback((e: React.PointerEvent) => {
    if (panning.current) {
      panning.current = false;
      return;
    }
    if (isDrawing()) {
      endTool();
      drawScene();
    }
  }, [drawScene]);

  const handleKeyDown = useCallback((e: KeyboardEvent) => {
    const target = e.target as HTMLElement;
    if (target && (target.tagName === "INPUT" || target.tagName === "TEXTAREA")) return;
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
    if (e.key === " " && activeTool === "hand" && spacePanning.current) {
      spacePanning.current = false;
      setActiveTool("select");
    }
  }, [activeTool, setActiveTool]);

  useEffect(() => {
    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("keyup", handleKeyUp);
    return () => { window.removeEventListener("keydown", handleKeyDown); window.removeEventListener("keyup", handleKeyUp); };
  }, [handleKeyDown, handleKeyUp]);

  const textEditingId = useEditorStore((s) => s.textEditingId);
  const editingEl = textEditingId ? elements.find((e) => e.id === textEditingId) : null;
  const editorOrigin = editingEl ? worldToScreen(editingEl.data.x ?? 0, editingEl.data.y ?? 0) : null;

  return (
    <div className="relative w-full h-full">
      <canvas
        ref={canvasRef}
        className="w-full h-full cursor-crosshair block"
        style={{ background: "#1a1a2e" }}
        onWheel={handleWheel}
        onPointerDown={handlePointerDown}
        onPointerMove={handlePointerMove}
        onPointerUp={handlePointerUp}
        onPointerLeave={handlePointerUp}
        onPointerCancel={handlePointerUp}
      />
      {editingEl && editorOrigin && (
        <TextEditorOverlay
          elId={editingEl.id}
          x={editorOrigin.x}
          y={editorOrigin.y}
          zoom={zoom}
          fontSize={editingEl.style.fontSize}
          fontFamily={editingEl.style.fontFamily}
          initial={editingEl.data.text as string}
        />
      )}
    </div>
  );
}

function TextEditorOverlay(props: {
  elId: string;
  x: number;
  y: number;
  zoom: number;
  fontSize: number;
  fontFamily: string;
  initial: string;
}) {
  const [value, setValue] = useState(props.initial ?? "");
  const ref = useRef<HTMLTextAreaElement>(null);
  const committed = useRef(false);

  useEffect(() => {
    const ta = ref.current;
    if (ta) {
      ta.focus();
      ta.select();
    }
  }, []);

  const commit = (keep: boolean) => {
    if (committed.current) return;
    committed.current = true;
    const store = useEditorStore.getState();
    const elId = props.elId;
    if (value.trim().length > 0) {
      const el = store.elements.find((e) => e.id === elId);
      if (el) store.updateElement(elId, { data: { ...el.data, text: value } });
    } else {
      store.removeElement(elId);
    }
    store.setTextEditingId(null);
    void keep;
  };

  return (
    <textarea
      ref={ref}
      value={value}
      onChange={(e) => setValue(e.target.value)}
      onBlur={() => commit(false)}
      onKeyDown={(e) => {
        if (e.key === "Escape") { e.preventDefault(); setValue(""); commit(false); }
        if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); commit(false); }
      }}
      className="absolute outline-none bg-transparent text-zinc-100 resize-none overflow-hidden"
      style={{
        left: props.x,
        top: props.y,
        width: Math.max(160 * props.zoom, 60),
        minHeight: props.fontSize * props.zoom,
        fontSize: props.fontSize * props.zoom,
        fontFamily: props.fontFamily,
        lineHeight: 1.3,
        transformOrigin: "0 0",
        whiteSpace: "pre-wrap",
        caretColor: "#3b82f6",
      }}
    />
  );
}
