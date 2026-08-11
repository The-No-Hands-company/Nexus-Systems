import { useEffect, useRef, useCallback, useState } from "react";
import rough from "roughjs";
import type { RoughCanvas } from "roughjs/bin/canvas";
import { useEditorStore, type Vec2 } from "../../stores/useEditorStore";
import { resolveStyleMode, type ElementData } from "../../stores/model";
import { renderElement } from "../../render/renderElement";
import { hitElement, elementBounds, resizeHandles, type Bounds, type Point } from "../../render/hitTest";
import { ToolController, draftToElement, snapToGrid, GRID_SIZE } from "../../tools/toolController";
import {
  translateElement,
  resizeElement,
  rotateElementTransform,
  currentRotation,
  invertTransformPoint,
  cloneElementOffset,
  elementInMarquee,
} from "../../tools/selection";

const GRID_COLOR = "rgba(255,255,255,0.06)";
const SELECTION_COLOR = "#3b82f6";
const MARQUEE_FILL = "rgba(59,130,246,0.08)";
const HANDLE_SCREEN_SIZE = 8;
const SELECT_TOLERANCE_PX = 6;

/** In-module clipboard buffer for copy/cut/paste (Ctrl+C/X/V). */
let clipboardBuffer: ElementData[] = [];

type DragMode = "move" | "resize" | "rotate" | "marquee";

interface DragState {
  mode: DragMode;
  startWorld: Point;
  /** Elements as they were at drag start; live updates are derived from this, not from `elements`. */
  snapshot: ElementData[];
  /** move: ids of every element being translated. */
  moveIds?: Set<string>;
  /** resize/rotate: the single element being manipulated. */
  targetId?: string;
  /** resize: which of the 8 box handles was grabbed. */
  handleId?: string;
  /** rotate: element-local center, and the mouse's starting angle around it (in the frame fixed at drag start). */
  rotateCenter?: Point;
  rotateStartAngle?: number;
  rotateInitialAngle?: number;
  /**
   * Whether `pushHistory()` has been called yet for this drag. Pushed lazily on
   * the first mousemove that actually produces a change — not at mousedown — so
   * a plain click-to-select (or grabbing a handle without moving, or grabbing a
   * resize handle on a shape resize doesn't support, e.g. freehand) never eats
   * an undo slot.
   */
  historyPushed?: boolean;
  /** marquee: selection to union the marquee's hits into at mouseup, when the drag started with shift held. */
  marqueeUnionIds?: Set<string>;
}

/** Pushes history once per drag, on first actual mutation — not at mousedown. */
function ensureHistoryPushed(drag: DragState): void {
  if (drag.historyPushed) return;
  useEditorStore.getState().pushHistory();
  drag.historyPushed = true;
}

/** Z-order helpers — reorder via the store's `reorderElements` so it's one undo step. */
function orderedIds(): string[] {
  return [...useEditorStore.getState().elements].sort((a, b) => a.order - b.order).map((el) => el.id);
}

function nudgeZOrder(direction: 1 | -1): void {
  const store = useEditorStore.getState();
  if (store.selectedElementIds.size === 0) return;
  const ids = orderedIds();
  const selected = store.selectedElementIds;
  if (direction === 1) {
    for (let i = ids.length - 2; i >= 0; i--) {
      if (selected.has(ids[i]) && !selected.has(ids[i + 1])) {
        [ids[i], ids[i + 1]] = [ids[i + 1], ids[i]];
      }
    }
  } else {
    for (let i = 1; i < ids.length; i++) {
      if (selected.has(ids[i]) && !selected.has(ids[i - 1])) {
        [ids[i], ids[i - 1]] = [ids[i - 1], ids[i]];
      }
    }
  }
  store.reorderElements(ids);
}

function bringSelectedToFront(): void {
  const store = useEditorStore.getState();
  if (store.selectedElementIds.size === 0) return;
  const ids = orderedIds();
  const selected = ids.filter((id) => store.selectedElementIds.has(id));
  const rest = ids.filter((id) => !store.selectedElementIds.has(id));
  store.reorderElements([...rest, ...selected]);
}

function sendSelectedToBack(): void {
  const store = useEditorStore.getState();
  if (store.selectedElementIds.size === 0) return;
  const ids = orderedIds();
  const selected = ids.filter((id) => store.selectedElementIds.has(id));
  const rest = ids.filter((id) => !store.selectedElementIds.has(id));
  store.reorderElements([...selected, ...rest]);
}

/** Deletes every selected element as a single undo step. */
function deleteSelectedElements(): void {
  const store = useEditorStore.getState();
  if (store.selectedElementIds.size === 0) return;
  store.pushHistory();
  store.setElementsLive(store.elements.filter((el) => !store.selectedElementIds.has(el.id)));
  store.setSelection([]);
}

/** Adds `newEls` to the board as a single undo step and selects them. */
function addClonesLive(newEls: ElementData[]): void {
  if (newEls.length === 0) return;
  const store = useEditorStore.getState();
  store.pushHistory();
  const base = store.elements.length;
  newEls.forEach((el, i) => {
    el.order = base + i;
  });
  store.setElementsLive([...store.elements, ...newEls]);
  store.setSelection(newEls.map((el) => el.id));
}

function copySelectedElements(): void {
  const store = useEditorStore.getState();
  const selected = store.elements.filter((el) => store.selectedElementIds.has(el.id));
  if (selected.length > 0) clipboardBuffer = selected.map((el) => structuredClone(el));
}

function cutSelectedElements(): void {
  copySelectedElements();
  deleteSelectedElements();
}

function pasteClipboard(): void {
  if (clipboardBuffer.length === 0) return;
  addClonesLive(clipboardBuffer.map((el) => cloneElementOffset(el, 16, 16)));
}

function duplicateSelectedElements(): void {
  const store = useEditorStore.getState();
  const selected = store.elements.filter((el) => store.selectedElementIds.has(el.id));
  addClonesLive(selected.map((el) => cloneElementOffset(el, 16, 16)));
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

    // Draw in the element's own local space so bounds + handles rotate with it
    // (matches the render loop, which applies the same transform before drawing).
    ctx.save();
    ctx.transform(el.transform.a, el.transform.b, el.transform.c, el.transform.d, el.transform.e, el.transform.f);

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
    ctx.restore();
  }
  ctx.restore();
}

/** Draws the live rubber-band marquee rect while dragging a selection box. */
function drawMarquee(ctx: CanvasRenderingContext2D, rect: Bounds, zoom: number): void {
  ctx.save();
  ctx.fillStyle = MARQUEE_FILL;
  ctx.strokeStyle = SELECTION_COLOR;
  ctx.lineWidth = 1 / zoom;
  ctx.setLineDash([4 / zoom, 3 / zoom]);
  ctx.fillRect(rect.x, rect.y, rect.width, rect.height);
  ctx.strokeRect(rect.x, rect.y, rect.width, rect.height);
  ctx.restore();
}

export default function Canvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const roughRef = useRef<RoughCanvas | null>(null);
  const ctxRef = useRef<CanvasRenderingContext2D | null>(null);
  const rafRef = useRef<number>(0);
  const panDragging = useRef(false);
  const lastMouse = useRef({ x: 0, y: 0 });
  const controllerRef = useRef(new ToolController());
  const dragRef = useRef<DragState | null>(null);
  const marqueeRef = useRef<Bounds | null>(null);
  const [textDraft, setTextDraft] = useState<{ worldX: number; worldY: number; value: string } | null>(null);

  const pan = useEditorStore((s) => s.pan);
  const zoom = useEditorStore((s) => s.zoom);
  const setPan = useEditorStore((s) => s.setPan);
  const setZoom = useEditorStore((s) => s.setZoom);
  const activeTool = useEditorStore((s) => s.activeTool);
  const board = useEditorStore((s) => s.board);
  const collabActive = useEditorStore((s) => s.collabActive);
  const selectElement = useEditorStore((s) => s.selectElement);
  const deselectAll = useEditorStore((s) => s.deselectAll);
  const setActiveTool = useEditorStore((s) => s.setActiveTool);

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
          if (el.data.hidden) continue; // layer panel visibility toggle
          // Elements are stored/hit-tested in local (untransformed) coordinates;
          // `transform` (currently always a pure rotation about the element's
          // center) is applied here at draw time, same as the overlay below.
          ctx2.save();
          ctx2.transform(el.transform.a, el.transform.b, el.transform.c, el.transform.d, el.transform.e, el.transform.f);
          renderElement(ctx2, rc, el, resolveStyleMode(el, boardDefault));
          ctx2.restore();
        }

        const draft = controllerRef.current.draft;
        if (draft) {
          const draftEl = draftToElement(draft, boardDefault);
          renderElement(ctx2, rc, draftEl, resolveStyleMode(draftEl, boardDefault));
        }

        drawSelectionOverlay(ctx2, store.elements, store.selectedElementIds, curZoom);
        if (marqueeRef.current) drawMarquee(ctx2, marqueeRef.current, curZoom);
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

      const gridSnap = board?.gridSnap ?? false;

      if (activeTool === "text") {
        const p = snapToGrid(world, GRID_SIZE, gridSnap);
        setTextDraft({ worldX: p.x, worldY: p.y, value: "" });
        return;
      }

      if (activeTool === "select") {
        const tol = SELECT_TOLERANCE_PX / zoom;
        const handleTol = tol + HANDLE_SCREEN_SIZE / zoom;
        const store = useEditorStore.getState();
        const shift = e.shiftKey || e.metaKey || e.ctrlKey;

        // 1. Resize/rotate handles on the currently selected elements take priority
        // over anything underneath them. Hit-test in each element's own local space
        // so a rotated element's (also-rotated) handles are grabbable where drawn.
        for (const el of store.elements) {
          if (!store.selectedElementIds.has(el.id)) continue;
          // An element can be selected first and locked/hidden afterwards from
          // the layer panel; its handles must stop responding immediately.
          if (el.data.locked || el.data.hidden) continue;
          const local = invertTransformPoint(el.transform, world);
          const bounds = elementBounds(el);
          const handle = resizeHandles(bounds).find((h) => Math.hypot(h.x - local.x, h.y - local.y) <= handleTol);
          if (!handle) continue;

          // No pushHistory() here — grabbing a handle isn't itself a mutation.
          // The drag lazily pushes history on its first real change (see
          // ensureHistoryPushed), so a click-then-release-without-moving (or
          // grabbing a resize handle on a shape resize doesn't support, e.g.
          // freehand) never eats an undo slot.
          if (handle.id === "rotate") {
            const cx = bounds.x + bounds.width / 2;
            const cy = bounds.y + bounds.height / 2;
            dragRef.current = {
              mode: "rotate",
              startWorld: world,
              snapshot: store.elements,
              targetId: el.id,
              rotateCenter: { x: cx, y: cy },
              rotateStartAngle: Math.atan2(local.y - cy, local.x - cx),
              rotateInitialAngle: currentRotation(el),
            };
          } else {
            dragRef.current = { mode: "resize", startWorld: world, snapshot: store.elements, targetId: el.id, handleId: handle.id };
          }
          return;
        }

        // 2. Click on an element: select it (shift toggles into a multi-select) and
        // prime a move drag for the whole resulting selection.
        const hit = [...store.elements]
          .reverse()
          .find((el) => hitElement(el, invertTransformPoint(el.transform, world), tol));
        if (hit) {
          if (shift) {
            selectElement(hit.id, true);
            return;
          }
          if (!store.selectedElementIds.has(hit.id)) {
            selectElement(hit.id, false);
          }
          const fresh = useEditorStore.getState();
          // No pushHistory() here either — a click that doesn't drag shouldn't
          // consume an undo slot; see ensureHistoryPushed.
          dragRef.current = {
            mode: "move",
            startWorld: world,
            snapshot: fresh.elements,
            moveIds: new Set(fresh.selectedElementIds),
          };
          return;
        }

        // 3. Empty canvas: deselect (unless shift-adding to a marquee) and start one.
        // When shift is held, remember the pre-drag selection so mouseup can union
        // the marquee's hits into it instead of replacing the selection outright.
        const preDragSelection = shift ? new Set(store.selectedElementIds) : undefined;
        if (!shift) deselectAll();
        dragRef.current = {
          mode: "marquee",
          startWorld: world,
          snapshot: store.elements,
          marqueeUnionIds: preDragSelection,
        };
        marqueeRef.current = { x: world.x, y: world.y, width: 0, height: 0 };
        return;
      }

      controllerRef.current.beginTool(activeTool, world, gridSnap, SELECT_TOLERANCE_PX / zoom);
    },
    [activeTool, screenToWorld, zoom, selectElement, deselectAll, board]
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

      const canvas = canvasRef.current;
      if (!canvas) return;
      const rect = canvas.getBoundingClientRect();
      const world = screenToWorld(e.clientX - rect.left, e.clientY - rect.top);

      const drag = dragRef.current;
      if (drag) {
        if (drag.mode === "marquee") {
          marqueeRef.current = {
            x: Math.min(drag.startWorld.x, world.x),
            y: Math.min(drag.startWorld.y, world.y),
            width: Math.abs(world.x - drag.startWorld.x),
            height: Math.abs(world.y - drag.startWorld.y),
          };
          return;
        }

        const dx = world.x - drag.startWorld.x;
        const dy = world.y - drag.startWorld.y;

        if (drag.mode === "move" && drag.moveIds) {
          if (dx === 0 && dy === 0) return; // no movement yet — nothing to commit
          ensureHistoryPushed(drag);
          const moveIds = drag.moveIds;
          const next = drag.snapshot.map((el) => (moveIds.has(el.id) ? translateElement(el, dx, dy) : el));
          useEditorStore.getState().setElementsLive(next);
          return;
        }

        if (drag.mode === "resize" && drag.targetId && drag.handleId) {
          if (dx === 0 && dy === 0) return;
          const { targetId, handleId } = drag;
          const original = drag.snapshot.find((el) => el.id === targetId);
          if (!original) return;
          const resizedTarget = resizeElement(original, handleId, dx, dy);
          // resizeElement returns the same reference for shapes it doesn't support
          // resizing (e.g. freehand) — that's a true no-op, not just "no movement
          // yet", so skip both the history push and the store update.
          if (resizedTarget === original) return;
          ensureHistoryPushed(drag);
          const next = drag.snapshot.map((el) => (el.id === targetId ? resizedTarget : el));
          useEditorStore.getState().setElementsLive(next);
          return;
        }

        if (drag.mode === "rotate" && drag.targetId && drag.rotateCenter) {
          const original = drag.snapshot.find((el) => el.id === drag.targetId);
          if (original) {
            // Invert with the *original* (drag-start) transform, which stays fixed
            // for the whole drag — this maps the live mouse position into the same
            // fixed local frame `rotateCenter`/`rotateStartAngle` were measured in.
            const local = invertTransformPoint(original.transform, world);
            const angleNow = Math.atan2(local.y - drag.rotateCenter.y, local.x - drag.rotateCenter.x);
            const delta = angleNow - (drag.rotateStartAngle ?? 0);
            // Pure radial mouse movement (toward/away from center) changes dx/dy
            // but not the angle — that's not a rotation yet, so don't commit it.
            if (delta === 0) return;
            const targetAngle = (drag.rotateInitialAngle ?? 0) + delta;
            ensureHistoryPushed(drag);
            const targetId = drag.targetId;
            const next = drag.snapshot.map((el) => (el.id === targetId ? rotateElementTransform(el, targetAngle) : el));
            useEditorStore.getState().setElementsLive(next);
          }
          return;
        }
        return;
      }

      controllerRef.current.updateTool(world, board?.gridSnap ?? false, SELECT_TOLERANCE_PX / zoom);
    },
    [pan, zoom, setPan, screenToWorld, board]
  );

  const handleMouseUp = useCallback(() => {
    if (panDragging.current) {
      panDragging.current = false;
      return;
    }

    const drag = dragRef.current;
    if (drag) {
      if (drag.mode === "marquee") {
        const rect = marqueeRef.current;
        if (rect && (rect.width > 2 || rect.height > 2)) {
          const store = useEditorStore.getState();
          // World-space AABB test (elementInMarquee), not the local-space
          // hitInMarquee — a rotated element's rendered footprint differs from
          // its local bounds, so the local test would miss/false-include it.
          const hitIds = store.elements
            .filter((el) => !el.data.locked && !el.data.hidden && elementInMarquee(el, rect))
            .map((el) => el.id);
          // Shift-drag unions into the pre-drag selection instead of replacing it.
          const finalIds = drag.marqueeUnionIds ? new Set([...drag.marqueeUnionIds, ...hitIds]) : new Set(hitIds);
          store.setSelection([...finalIds]);
        }
        marqueeRef.current = null;
      }
      dragRef.current = null;
      return;
    }

    controllerRef.current.endTool();
  }, []);

  const commitTextDraft = useCallback(() => {
    if (textDraft && textDraft.value.length > 0) {
      controllerRef.current.commitText(textDraft.worldX, textDraft.worldY, textDraft.value);
    }
    setTextDraft(null);
  }, [textDraft]);

  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      const target = e.target as HTMLElement | null;
      if (target && (target.tagName === "TEXTAREA" || target.tagName === "INPUT")) {
        if (e.key === "Escape") setTextDraft(null);
        return;
      }
      if (e.key === "Escape") {
        controllerRef.current.cancel();
        return;
      }
      if (e.key === " " && !e.repeat) {
        setActiveTool("hand");
        e.preventDefault();
      }

      const mod = e.metaKey || e.ctrlKey;

      // Tool shortcuts (V/H/P/R/E/L/A/T/S) — guarded by !mod so Ctrl+V (paste),
      // Ctrl+A, etc. don't also swap the active tool as a side effect.
      if (!mod) {
        if (e.key === "v") setActiveTool("select");
        if (e.key === "h") setActiveTool("hand");
        if (e.key === "p") setActiveTool("pen");
        if (e.key === "r") setActiveTool("rectangle");
        if (e.key === "o") setActiveTool("ellipse");
        if (e.key === "l") setActiveTool("line");
        if (e.key === "a") setActiveTool("arrow");
        if (e.key === "t") setActiveTool("text");
        if (e.key === "s") setActiveTool("sticky");
        if (e.key === "e") setActiveTool("eraser");
      }

      // Undo/redo: Ctrl+Z undo, Ctrl+Shift+Z or Ctrl+Y redo (Ctrl+U kept as a
      // pre-existing undo alias).
      if (mod && e.key === "u" && !e.shiftKey) {
        e.preventDefault();
        useEditorStore.getState().undo();
      }
      if (mod && e.key === "z" && !e.shiftKey) {
        e.preventDefault();
        useEditorStore.getState().undo();
      }
      if (mod && ((e.key === "z" && e.shiftKey) || e.key === "y")) {
        e.preventDefault();
        useEditorStore.getState().redo();
      }

      if (mod && e.key === "0") {
        setZoom(1);
        setPan({ x: 0, y: 0 });
      }

      // Delete/Backspace: remove every selected element as one undo step.
      if (!mod && (e.key === "Delete" || e.key === "Backspace")) {
        if (useEditorStore.getState().selectedElementIds.size > 0) {
          e.preventDefault();
          deleteSelectedElements();
        }
      }

      // Clipboard: Ctrl+C copy, Ctrl+X cut, Ctrl+V paste, Ctrl+D duplicate.
      if (mod && e.key === "c") {
        copySelectedElements();
      }
      if (mod && e.key === "x") {
        if (useEditorStore.getState().selectedElementIds.size > 0) {
          e.preventDefault();
          cutSelectedElements();
        }
      }
      if (mod && e.key === "v") {
        if (clipboardBuffer.length > 0) {
          e.preventDefault();
          pasteClipboard();
        }
      }
      if (mod && e.key === "d") {
        if (useEditorStore.getState().selectedElementIds.size > 0) {
          e.preventDefault();
          duplicateSelectedElements();
        }
      }

      // Z-order: ] / [ nudge one step, Shift+] / Shift+[ jump to front/back.
      // Keyed off e.code so Shift (which remaps "]" to "}" on US layouts) doesn't
      // break detection.
      if (!mod && e.code === "BracketRight") {
        e.preventDefault();
        if (e.shiftKey) bringSelectedToFront();
        else nudgeZOrder(1);
      }
      if (!mod && e.code === "BracketLeft") {
        e.preventDefault();
        if (e.shiftKey) sendSelectedToBack();
        else nudgeZOrder(-1);
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
    <>
      <div className="absolute top-2 right-2 z-10 flex items-center gap-1.5 px-2 py-1 rounded-md text-[11px] font-medium bg-zinc-900/80 text-zinc-300 border border-zinc-700 pointer-events-none">
        <span className={`h-2 w-2 rounded-full ${collabActive ? "bg-emerald-400" : "bg-zinc-500"}`} />
        {collabActive ? "Live" : "Offline"}
      </div>
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
      {textDraft && (
        <textarea
          autoFocus
          value={textDraft.value}
          onChange={(e) => setTextDraft({ ...textDraft, value: e.target.value })}
          onBlur={commitTextDraft}
          onKeyDown={(e) => {
            if (e.key === "Escape") {
              e.preventDefault();
              commitTextDraft();
            }
          }}
          style={{
            position: "absolute",
            left: (textDraft.worldX + pan.x) * zoom,
            top: (textDraft.worldY + pan.y) * zoom,
            minWidth: 120,
            minHeight: 28,
            fontSize: 20 * zoom,
          }}
          className="bg-transparent border border-blue-400 text-zinc-100 font-sans outline-none p-1 resize"
        />
      )}
    </>
  );
}
