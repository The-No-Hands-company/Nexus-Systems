import { useEditorStore } from "../stores/useEditorStore";
import { elementBounds } from "../render/geometry";
import { useState } from "react";

export default function LayerPanel() {
  const elements = useEditorStore((s) => s.elements);
  const selectedElementIds = useEditorStore((s) => s.selectedElementIds);
  const selectElement = useEditorStore((s) => s.selectElement);
  const removeElement = useEditorStore((s) => s.removeElement);
  const updateElement = useEditorStore((s) => s.updateElement);
  const reorderElements = useEditorStore((s) => s.reorderElements);
  const [renamingId, setRenamingId] = useState<string | null>(null);

  const sorted = [...elements].sort((a, b) => b.order - a.order);

  const oneDown = (id: string) => {
    const cur = elements.find((e) => e.id === id);
    if (!cur) return;
    const above = elements.filter((e) => e.order < cur.order);
    const below = elements.filter((e) => e.order > cur.order);
    if (above.length === 0) return;
    const swapWith = above.sort((a, b) => b.order - a.order)[0];
    reorderElements([...below.map((e) => e.id), id, swapWith.id, ...above.filter((e) => e.id !== swapWith.id).map((e) => e.id)]);
  };

  const oneUp = (id: string) => {
    const cur = elements.find((e) => e.id === id);
    if (!cur) return;
    const above = elements.filter((e) => e.order > cur.order);
    const below = elements.filter((e) => e.order < cur.order);
    if (above.length === 0) return;
    const swapWith = above.sort((a, b) => a.order - b.order)[0];
    reorderElements([...below.map((e) => e.id), swapWith.id, id, ...above.filter((e) => e.id !== swapWith.id).map((e) => e.id)]);
  };

  return (
    <div className="flex flex-col h-full">
      <div className="p-2 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800">
        Layers
      </div>
      <div className="flex-1 overflow-y-auto p-1 space-y-1">
        {sorted.length === 0 && (
          <div className="p-3 text-xs text-zinc-600 text-center">No elements</div>
        )}
        {sorted.map((el) => {
          const hidden = !!el.data.hidden;
          const locked = !!el.data.locked;
          const name = (el.data.name as string) ?? el.elementType;
          const b = elementBounds(el);
          return (
            <div
              key={el.id}
              onClick={(e) => selectElement(el.id, e.metaKey || e.ctrlKey)}
              className={`flex items-center gap-1.5 px-2 py-1 rounded cursor-pointer text-sm transition-colors
                ${selectedElementIds.has(el.id) ? "bg-blue-600/20 text-blue-300" : "hover:bg-zinc-800 text-zinc-300"}
                ${hidden ? "opacity-40" : ""}`}
            >
              <button
                onClick={(e) => { e.stopPropagation(); updateElement(el.id, { data: { ...el.data, hidden: !hidden } }); }}
                title="Hide / show"
                className="w-4 text-center text-xs shrink-0"
              >
                {hidden ? "–" : "👁"}
              </button>
              <button
                onClick={(e) => { e.stopPropagation(); updateElement(el.id, { data: { ...el.data, locked: !locked } }); }}
                title="Lock"
                className="w-4 text-center text-xs shrink-0"
              >
                {locked ? "🔒" : "🔓"}
              </button>
              <span className="flex-1 truncate" onDoubleClick={(e) => { e.stopPropagation(); setRenamingId(el.id); }}>
                {renamingId === el.id ? (
                  <RenameInput
                    initial={(el.data.name as string) ?? el.elementType}
                    onCommit={(name) => { updateElement(el.id, { data: { ...el.data, name } }); setRenamingId(null); }}
                    onCancel={() => setRenamingId(null)}
                  />
                ) : name}
              </span>
              <span className="text-[10px] text-zinc-600 uppercase shrink-0">{el.elementType}</span>
              <span className="text-[10px] text-zinc-700 shrink-0">({b.x | 0},{b.y | 0})</span>
              <button onClick={(e) => { e.stopPropagation(); removeElement(el.id); }}
                className="w-4 text-center text-xs text-zinc-600 hover:text-red-400 shrink-0">✕</button>
            </div>
          );
        })}
      </div>
      <div className="p-2 border-t border-zinc-800 space-y-1">
        <div className="flex gap-1">
          <button
            onClick={() => { const sel = [...selectedElementIds]; if (sel.length === 1) oneUp(sel[0]); }}
            className="flex-1 px-2 py-1 text-xs bg-zinc-800 hover:bg-zinc-700 text-zinc-300 rounded"
            disabled={selectedElementIds.size !== 1}
          >
            ↑
          </button>
          <button
            onClick={() => { const sel = [...selectedElementIds]; if (sel.length === 1) oneDown(sel[0]); }}
            className="flex-1 px-2 py-1 text-xs bg-zinc-800 hover:bg-zinc-700 text-zinc-300 rounded"
            disabled={selectedElementIds.size !== 1}
          >
            ↓
          </button>
        </div>
        <button
          onClick={() => { selectedElementIds.forEach((id) => removeElement(id)); }}
          className="w-full px-2 py-1 text-xs bg-red-900/40 hover:bg-red-800/60 text-red-400 rounded"
          disabled={selectedElementIds.size === 0}
        >
          Delete Selected
        </button>
      </div>
    </div>
  );
}

function RenameInput({ initial, onCommit, onCancel }: { initial: string; onCommit: (v: string) => void; onCancel: () => void }) {
  const [val, setVal] = useState(initial);
  return (
    <input
      autoFocus
      value={val}
      onChange={(e) => setVal(e.target.value)}
      onBlur={() => (val.trim() ? onCommit(val.trim()) : onCancel())}
      onKeyDown={(e) => {
        if (e.key === "Enter") onCommit(val.trim() || initial);
        if (e.key === "Escape") onCancel();
      }}
      className="bg-zinc-700 text-zinc-100 text-xs rounded px-1 py-0 w-full outline-none"
    />
  );
}
