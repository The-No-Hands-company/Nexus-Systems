import { useEditorStore } from "../stores/useEditorStore";

export default function LayerPanel() {
  const project = useEditorStore((s) => s.project);
  const selectedLayerIds = useEditorStore((s) => s.selectedLayerIds);
  const selectLayer = useEditorStore((s) => s.selectLayer);
  const updateLayer = useEditorStore((s) => s.updateLayer);
  const removeLayer = useEditorStore((s) => s.removeLayer);
  const reorderLayer = useEditorStore((s) => s.reorderLayer);

  if (!project) {
    return (
      <div className="p-4 text-sm text-zinc-500 flex items-center justify-center h-full">
        No project loaded
      </div>
    );
  }

  const layers = [...project.layers].sort((a, b) => b.order - a.order);

  return (
    <aside aria-label="Layers" className="flex flex-col h-full">
      <div className="p-2 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800">
        Layers
      </div>
      <div className="flex-1 overflow-y-auto p-1 space-y-1">
        {layers.map((layer) => (
          <div
            key={layer.id}
            className={`flex items-center gap-2 px-2 py-1.5 rounded text-sm transition-colors
              ${selectedLayerIds.has(layer.id) ? "bg-accent/20 text-accent" : "text-zinc-300"}`}
          >
            <button
              type="button"
              onClick={(e) => { e.stopPropagation(); updateLayer(layer.id, { visible: !layer.visible }); }}
              aria-label={`${layer.visible ? "Hide" : "Show"} ${layer.name}`}
              className="min-h-[44px] min-w-[44px] rounded text-center text-xs hover:bg-zinc-800 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
            >
              {layer.visible ? "👁" : "—"}
            </button>
            <button
              type="button"
              onClick={(e) => selectLayer(layer.id, e.metaKey || e.ctrlKey)}
              aria-pressed={selectedLayerIds.has(layer.id)}
              className="flex-1 min-h-[44px] truncate text-left rounded focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
            >{layer.name}</button>
            <span className="text-[10px] text-zinc-600 uppercase">{layer.type}</span>
          </div>
        ))}
      </div>
      <div className="p-2 border-t border-zinc-800 flex gap-1">
        <button
          type="button"
          onClick={() => {
            const id = crypto.randomUUID();
            const order = project.layers.length;
            useEditorStore.getState().addLayer({
              id, name: `Layer ${order + 1}`, type: "shape",
              visible: true, locked: false, opacity: 1, order,
              transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
              data: { width: 100, height: 100, x: 0, y: 0, fill: "#3b82f6" },
            });
          }}
          aria-label="Add layer"
          className="flex-1 min-h-[44px] px-2 py-1 text-xs bg-zinc-800 hover:bg-zinc-700 rounded focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
        >
          + Add
        </button>
        <button
          type="button"
          onClick={() => {
            selectedLayerIds.forEach((id) => removeLayer(id));
          }}
          aria-label="Delete selected layers"
          className="min-h-[44px] px-2 py-1 text-xs bg-state-danger/20 hover:bg-state-danger/30 text-state-danger rounded focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
          disabled={selectedLayerIds.size === 0}
        >
          Del
        </button>
      </div>
    </aside>
  );
}
