import { useEffect, useRef, useCallback, useState } from "react";
import rough from "roughjs";
import { useEditorStore } from "../../stores/useEditorStore";
import { resolveStyleMode } from "../../stores/model";
import { renderElement } from "../../render/renderElement";
import { elementBounds, resizeHandles } from "../../render/geometry";
import { beginTool, updateTool, endTool, getPreview, isDrawing, type ToolName } from "../../tools/toolController";
import {
  isOnHandle, topmostHit, beginMove, beginResize, beginRotate, beginMarquee,
  updateSel, endSel, cancelSel, getSelPreview, isSelecting, bringForward, sendBackward,
} from "../../tools/selection";

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
  const clipboard = useRef<any[]>([]);

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

    const selPreview = getSelPreview();
    if (selPreview) {
      ctx.save();
      ctx.globalAlpha = 0.6;
      if (selPreview.type === "marquee") {
        ctx.strokeStyle = "#3b82f6";
        ctx.setLineDash([6 / zoom, 4 / zoom]);
        ctx.lineWidth = 1.5 / zoom;
        ctx.strokeRect(selPreview.rect.x, selPreview.rect.y, selPreview.rect.width, selPreview.rect.height);
        ctx.fillStyle = "rgba(59,130,246,0.08)";
        ctx.fillRect(selPreview.rect.x, selPreview.rect.y, selPreview.rect.width, selPreview.rect.height);
      } else if (selPreview.type === "box") {
        const els = selPreview.els ?? (selPreview.el ? [selPreview.el] : []);
        for (const pEl of els) {
          renderElement(ctx, rc, pEl, resolveStyleMode(pEl, board?.defaultStyleMode ?? "clean"));
        }
      }
      ctx.restore();
    }

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
      const sel = [...elements].filter((el) => selectedElementIds.has(el.id));
      if (sel.length > 0) {
        const top = [...sel].sort((a, b) => b.order - a.order)[0];
        const handle = isOnHandle(top, world, 6 / zoom);
        if (handle) {
          if (handle === "rotate") beginRotate(top, world);
          else beginResize(top, handle, world);
          drawScene();
          return;
        }
      }
      const hit = topmostHit(elements, world, 4 / zoom);
      if (hit) {
        if (!selectedElementIds.has(hit.id)) selectElement(hit.id, e.shiftKey || e.metaKey || e.ctrlKey);
        beginMove(elements.filter((el) => selectedElementIds.has(el.id) || el.id === hit.id), world);
        drawScene();
      } else {
        if (!e.shiftKey) deselectAll();
        beginMarquee(world);
        drawScene();
      }
      return;
    }

    if (isDrawnTool(activeTool)) {
      beginTool(activeTool, world);
      updateTool(world);
      drawScene();
    }
  }, [activeTool, screenToWorld, elements, selectedElementIds, selectElement, deselectAll, drawScene]);

  const handlePointerMove = useCallback((e: React.PointerEvent) => {
    if (panning.current) {
      const dx = e.clientX - lastMouse.current.x;
      const dy = e.clientY - lastMouse.current.y;
      setPan({ x: pan.x + dx / zoom, y: pan.y + dy / zoom });
      lastMouse.current = { x: e.clientX, y: e.clientY };
      return;
    }
    const rect = canvasRef.current!.getBoundingClientRect();
    const world = screenToWorld(e.clientX - rect.left, e.clientY - rect.top);
    if (isSelecting()) {
      updateSel(world);
      drawScene();
    } else if (isDrawing()) {
      updateTool(world);
      drawScene();
    }
  }, [pan, zoom, setPan, screenToWorld, drawScene]);

  const handlePointerUp = useCallback((e: React.PointerEvent) => {
    if (panning.current) { panning.current = false; return; }
    if (isSelecting()) {
      const res = endSel(elements);
      const store = useEditorStore.getState();
      if (res.updates.length > 0) {
        res.updates.forEach((u) => {
          store.updateElement(u.id, u.data ? { data: u.data } : { transform: u.transform! });
        });
      } else if (res.marqueeSelect.length > 0) {
        res.marqueeSelect.forEach((id) => selectElement(id, true));
      }
      drawScene();
      return;
    }
    if (isDrawing()) {
      endTool();
      drawScene();
    }
  }, [drawScene, elements, selectElement]);

  const doDelete = useCallback(() => {
    const store = useEditorStore.getState();
    [...store.selectedElementIds].forEach((id) => store.removeElement(id));
  }, []);

  const doDuplicate = useCallback(() => {
    const store = useEditorStore.getState();
    const selected = store.elements.filter((el) => store.selectedElementIds.has(el.id));
    if (selected.length === 0) return;
    const copies = selected.map((el) => ({
      ...JSON.parse(JSON.stringify(el)),
      id: crypto.randomUUID(),
      data: { ...el.data, x: (el.data.x ?? 0) + 24, y: (el.data.y ?? 0) + 24 },
    }));
    copies.forEach((c) => store.addElement(c));
    store.deselectAll();
    copies.forEach((c) => selectElement(c.id, true));
  }, [selectElement]);

  const doCopy = useCallback(() => {
    const store = useEditorStore.getState();
    clipboard.current = store.elements.filter((el) => store.selectedElementIds.has(el.id)).map((el) => JSON.parse(JSON.stringify(el)));
  }, []);

  const doPaste = useCallback(() => {
    const store = useEditorStore.getState();
    if (clipboard.current.length === 0) return;
    const copies = clipboard.current.map((el) => ({
      ...JSON.parse(JSON.stringify(el)),
      id: crypto.randomUUID(),
      data: { ...el.data, x: (el.data.x ?? 0) + 24, y: (el.data.y ?? 0) + 24 },
    }));
    copies.forEach((c) => store.addElement(c));
    store.deselectAll();
    copies.forEach((c) => selectElement(c.id, true));
  }, [selectElement]);

  const handleKeyDown = useCallback((e: KeyboardEvent) => {
    const target = e.target as HTMLElement;
    if (target && (target.tagName === "INPUT" || target.tagName === "TEXTAREA")) return;

    if ((e.metaKey || e.ctrlKey) && e.key === "c") { e.preventDefault(); doCopy(); return; }
    if ((e.metaKey || e.ctrlKey) && e.key === "v") { e.preventDefault(); doPaste(); return; }
    if ((e.metaKey || e.ctrlKey) && e.key === "d") { e.preventDefault(); doDuplicate(); return; }
    if ((e.key === "Delete" || e.key === "Backspace")) { e.preventDefault(); doDelete(); return; }
    if (e.key === "]") { useEditorStore.getState().reorderElements(bringForward(useEditorStore.getState().elements, useEditorStore.getState().selectedElementIds)); return; }
    if (e.key === "[") { useEditorStore.getState().reorderElements(sendBackward(useEditorStore.getState().elements, useEditorStore.getState().selectedElementIds)); return; }

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
  }, [setActiveTool, setZoom, setPan, doCopy, doPaste, doDuplicate, doDelete]);

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

  useEffect(() => {
    return () => cancelSel();
  }, []);

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
    if (ta) { ta.focus(); ta.select(); }
  }, []);

  const commit = () => {
    if (committed.current) return;
    committed.current = true;
    const store = useEditorStore.getState();
    if (value.trim().length > 0) {
      const el = store.elements.find((e) => e.id === props.elId);
      if (el) store.updateElement(props.elId, { data: { ...el.data, text: value } });
    } else {
      store.removeElement(props.elId);
    }
    store.setTextEditingId(null);
  };

  return (
    <textarea
      ref={ref}
      value={value}
      onChange={(e) => setValue(e.target.value)}
      onBlur={() => commit()}
      onKeyDown={(e) => {
        if (e.key === "Escape") { e.preventDefault(); setValue(""); commit(); }
        if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); commit(); }
      }}
      className="absolute outline-none bg-zinc-800/40 rounded px-1 py-0.5 text-zinc-100 resize-none overflow-hidden"
      style={{
        left: props.x,
        top: props.y,
        width: 220 * props.zoom,
        minHeight: props.fontSize * props.zoom,
        fontSize: props.fontSize * props.zoom,
        fontFamily: props.fontFamily,
        lineHeight: 1.3,
        whiteSpace: "pre-wrap",
        caretColor: "#3b82f6",
      }}
    />
  );
}
