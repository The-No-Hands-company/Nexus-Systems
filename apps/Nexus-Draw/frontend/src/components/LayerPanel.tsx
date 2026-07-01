import { useEditorStore } from "../stores/useEditorStore";

export default function LayerPanel() {
  const elements = useEditorStore((s) => s.elements);
  const selectedElementIds = useEditorStore((s) => s.selectedElementIds);
  const selectElement = useEditorStore((s) => s.selectElement);
  const removeElement = useEditorStore((s) => s.removeElement);
  const updateElement = useEditorStore((s) => s.updateElement);

  const sorted = [...elements].sort((a, b) => b.order - a.order);

  return (
    <div className="flex flex-col h-full">
      <div className="p-2 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800">
        Elements
      </div>
      <div className="flex-1 overflow-y-auto p-1 space-y-1">
        {sorted.length === 0 && (
          <div className="p-3 text-xs text-zinc-600 text-center">No elements</div>
        )}
        {sorted.map((el) => (
          <div
            key={el.id}
            onClick={(e) => selectElement(el.id, e.metaKey || e.ctrlKey)}
            className={`flex items-center gap-2 px-2 py-1.5 rounded cursor-pointer text-sm transition-colors
              ${selectedElementIds.has(el.id) ? "bg-blue-600/20 text-blue-300" : "hover:bg-zinc-800 text-zinc-300"}`}
          >
            <button
              onClick={(e) => { e.stopPropagation(); updateElement(el.id, { style: { ...el.style, visible: !(el.style as any).visible } }); }}
              className="w-4 text-center text-xs"
            >
              {(el.style as any).visible === false ? "—" : "👁"}
            </button>
            <span className="flex-1 truncate">{el.elementType}</span>
            <span className="text-[10px] text-zinc-600 uppercase">{el.elementType}</span>
          </div>
        ))}
      </div>
      <div className="p-2 border-t border-zinc-800">
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
