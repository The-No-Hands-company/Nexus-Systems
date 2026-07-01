import { useState, useCallback, useRef, useEffect } from "react";
import { useEditorStore } from "../stores/useEditorStore";
import Canvas from "./Canvas/Canvas";

export default function PhotoViewer() {
  const viewingPhoto = useEditorStore((s) => s.viewingPhoto);
  const [zoom, setZoom] = useState(1);
  const [pan, setPan] = useState({ x: 0, y: 0 });
  const dragging = useRef(false);
  const lastMouse = useRef({ x: 0, y: 0 });

  const handleWheel = useCallback(
    (e: React.WheelEvent) => {
      e.preventDefault();
      const delta = -e.deltaY * 0.001;
      setZoom((z) => Math.max(0.1, Math.min(20, z * (1 + delta))));
    },
    [],
  );

  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    dragging.current = true;
    lastMouse.current = { x: e.clientX, y: e.clientY };
  }, []);

  const handleMouseMove = useCallback(
    (e: React.MouseEvent) => {
      if (!dragging.current) return;
      const dx = e.clientX - lastMouse.current.x;
      const dy = e.clientY - lastMouse.current.y;
      setPan((p) => ({ x: p.x + dx, y: p.y + dy }));
      lastMouse.current = { x: e.clientX, y: e.clientY };
    },
    [],
  );

  const handleMouseUp = useCallback(() => {
    dragging.current = false;
  }, []);

  if (!viewingPhoto) return null;

  return (
    <div className="w-full h-full flex flex-col">
      <div className="flex items-center justify-between px-3 py-1.5 bg-zinc-900 border-b border-zinc-800 shrink-0">
        <span className="text-xs text-zinc-400">{viewingPhoto.title || "untitled"}</span>
        <div className="flex items-center gap-2 text-xs text-zinc-500">
          <button onClick={() => setZoom((z) => z - 0.25)} className="hover:text-zinc-200">−</button>
          <span>{Math.round(zoom * 100)}%</span>
          <button onClick={() => setZoom((z) => z + 0.25)} className="hover:text-zinc-200">+</button>
          <button onClick={() => { setZoom(1); setPan({ x: 0, y: 0 }); }} className="hover:text-zinc-200 ml-1">Fit</button>
        </div>
      </div>
      <div
        className="flex-1 overflow-hidden cursor-grab active:cursor-grabbing"
        onWheel={handleWheel}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
      >
        <Canvas />
      </div>
    </div>
  );
}
