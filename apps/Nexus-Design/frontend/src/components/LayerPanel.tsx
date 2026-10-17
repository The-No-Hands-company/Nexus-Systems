import { useEditorStore } from "../stores/useEditorStore";

export default function LayerPanel() {
  const frames = useEditorStore((s) => s.frames);
  const selectedFrameId = useEditorStore((s) => s.selectedFrameId);
  const selectFrame = useEditorStore((s) => s.selectFrame);
  const selectedLayerIds = useEditorStore((s) => s.selectedLayerIds);
  const selectLayer = useEditorStore((s) => s.selectLayer);
  const updateLayer = useEditorStore((s) => s.updateLayer);
  const removeLayer = useEditorStore((s) => s.removeLayer);
  const reorderLayer = useEditorStore((s) => s.reorderLayer);

  const selectedFrame = frames.find((f) => f.id === selectedFrameId);

  return (
    <aside aria-label="Layers" className="flex flex-col h-full">
      <div className="p-2 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800">
        Layers
      </div>
      <div className="flex-1 overflow-y-auto p-1 space-y-1">
        {frames.map((frame) => (
          <div key={frame.id}>
            <button
              onClick={() => selectFrame(frame.id)}
              aria-pressed={selectedFrameId === frame.id}
              className={`w-full min-h-[44px] flex items-center gap-2 px-2 py-1.5 rounded text-xs transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent
                ${selectedFrameId === frame.id ? "bg-accent/20 text-accent" : "hover:bg-zinc-800 text-zinc-400"}`}
            >
              <span className={`${selectedFrameId === frame.id ? "text-accent" : "text-zinc-600"}`}>▸</span>
              {frame.name}
            </button>
            {selectedFrameId === frame.id && frame.layers.map((layer) => (
              <div
                key={layer.id}
                className={`flex items-center gap-2 px-2 py-1.5 ml-3 rounded text-sm transition-colors
                  ${selectedLayerIds.has(layer.id) ? "bg-accent/20 text-accent" : "text-zinc-300"}`}
              >
                <button
                  type="button"
                  onClick={(e) => { e.stopPropagation(); updateLayer(frame.id, layer.id, { visible: !layer.visible }); }}
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
        ))}
      </div>
      <div className="p-2 border-t border-zinc-800 flex gap-1">
        <button
          type="button"
          onClick={() => {
            if (!selectedFrameId) return;
            const id = crypto.randomUUID();
            const frame = frames.find((f) => f.id === selectedFrameId);
            const order = frame ? frame.layers.length : 0;
            useEditorStore.getState().addLayer(selectedFrameId, {
              id, name: `Layer ${order + 1}`, type: "shape",
              visible: true, locked: false, opacity: 1, order,
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
            if (!selectedFrameId) return;
            selectedLayerIds.forEach((id) => removeLayer(selectedFrameId, id));
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
