import { useEditorStore } from "../stores/useEditorStore";

export default function TopBar() {
  const board = useEditorStore((s) => s.board);
  const zoom = useEditorStore((s) => s.zoom);
  const setZoom = useEditorStore((s) => s.setZoom);
  const setPan = useEditorStore((s) => s.setPan);
  const undo = useEditorStore((s) => s.undo);
  const redo = useEditorStore((s) => s.redo);
  const undoStack = useEditorStore((s) => s.undoStack);
  const redoStack = useEditorStore((s) => s.redoStack);

  return (
    <div className="flex items-center justify-between h-10 px-4 bg-zinc-900 border-b border-zinc-800 shrink-0">
      <div className="flex items-center gap-3">
        <span className="font-semibold text-sm text-zinc-200">Nexus-Draw</span>
        {board && (
          <span className="text-sm text-zinc-400">{board.name}</span>
        )}
      </div>
      <div className="flex items-center gap-2 text-xs text-zinc-500">
        <button onClick={undo} disabled={undoStack.length === 0} className="hover:text-zinc-200 disabled:opacity-30">↩</button>
        <button onClick={redo} disabled={redoStack.length === 0} className="hover:text-zinc-200 disabled:opacity-30">↪</button>
        <span className="text-zinc-700">|</span>
        <button onClick={() => setZoom(zoom - 0.1)} className="hover:text-zinc-200">−</button>
        <span>{Math.round(zoom * 100)}%</span>
        <button onClick={() => setZoom(zoom + 0.1)} className="hover:text-zinc-200">+</button>
        <button onClick={() => { setZoom(1); setPan({ x: 0, y: 0 }); }} className="hover:text-zinc-200 ml-1">
          Fit
        </button>
      </div>
    </div>
  );
}
