import { useEditorStore } from "../stores/useEditorStore";
import { downloadPNG, downloadSVG } from "../utils/export";
import * as api from "../utils/api";
import HelpOverlay from "./HelpOverlay";

export default function TopBar({ embedded = false }: { embedded?: boolean }) {
  const board = useEditorStore((s) => s.board);
  const elements = useEditorStore((s) => s.elements);
  const zoom = useEditorStore((s) => s.zoom);
  const setZoom = useEditorStore((s) => s.setZoom);
  const setPan = useEditorStore((s) => s.setPan);
  const undo = useEditorStore((s) => s.undo);
  const redo = useEditorStore((s) => s.redo);
  const undoStack = useEditorStore((s) => s.undoStack);
  const redoStack = useEditorStore((s) => s.redoStack);

  const doExport = (kind: "png" | "svg") => {
    const b = useEditorStore.getState().board;
    if (!b) return;
    const els = useEditorStore.getState().elements.filter((el) => !el.data.hidden);
    if (kind === "png") downloadPNG(b, els);
    else downloadSVG(b, els);
  };

  const save = async () => {
    const st = useEditorStore.getState();
    if (!st.board) return;
    await api.saveBoard(st.board.id, st.board, st.elements);
  };

  return (
    <header role="banner" className={`flex flex-wrap items-center ${embedded ? "justify-end" : "justify-between"} gap-y-1 min-h-[2.5rem] px-3 py-1 bg-zinc-900 border-b border-zinc-800 shrink-0`}>
      {!embedded && (
        <div className="flex items-center gap-2 min-w-0">
          <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true" className="text-accent shrink-0">
            <path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/>
          </svg>
          <span className="font-semibold text-sm text-zinc-200 truncate">Nexus-Draw</span>
          {board && (
            <span className="text-sm text-zinc-400 truncate max-w-[8rem]">{board.name}</span>
          )}
        </div>
      )}
      <div className="flex items-center gap-1.5 text-xs text-zinc-500 flex-wrap">
        <button onClick={undo} disabled={!board || undoStack.length === 0} aria-label="Undo" className="px-1.5 py-1 rounded hover:text-zinc-200 hover:bg-zinc-800 disabled:opacity-30 focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-accent">↩</button>
        <button onClick={redo} disabled={!board || redoStack.length === 0} aria-label="Redo" className="px-1.5 py-1 rounded hover:text-zinc-200 hover:bg-zinc-800 disabled:opacity-30 focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-accent">↪</button>
        <span className="text-zinc-700" aria-hidden="true">|</span>
        <button onClick={save} disabled={!board} className="px-1.5 py-1 rounded hover:text-zinc-200 hover:bg-zinc-800 disabled:opacity-30 focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-accent">Save</button>
        <span className="text-zinc-700 hidden sm:inline" aria-hidden="true">|</span>
        <button onClick={() => doExport("png")} disabled={!board} aria-label="Export PNG" className="hidden sm:inline-block px-1.5 py-1 rounded hover:text-zinc-200 hover:bg-zinc-800 disabled:opacity-30 focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-accent">⇩ PNG</button>
        <button onClick={() => doExport("svg")} disabled={!board} aria-label="Export SVG" className="hidden sm:inline-block px-1.5 py-1 rounded hover:text-zinc-200 hover:bg-zinc-800 disabled:opacity-30 focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-accent">⇩ SVG</button>
        <span className="text-zinc-700" aria-hidden="true">|</span>
        <button onClick={() => setZoom(zoom - 0.1)} aria-label="Zoom out" className="px-1.5 py-1 rounded hover:text-zinc-200 hover:bg-zinc-800 focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-accent">−</button>
        <span aria-live="polite" aria-label={`Zoom ${Math.round(zoom * 100)} percent`}>{Math.round(zoom * 100)}%</span>
        <button onClick={() => setZoom(zoom + 0.1)} aria-label="Zoom in" className="px-1.5 py-1 rounded hover:text-zinc-200 hover:bg-zinc-800 focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-accent">+</button>
        <button onClick={() => { setZoom(1); setPan({ x: 0, y: 0 }); }} aria-label="Fit to view" className="px-1.5 py-1 rounded hover:text-zinc-200 hover:bg-zinc-800 focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-accent">
          Fit
        </button>
        <HelpOverlay />
      </div>
    </header>
  );
}
