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
    <div className="flex flex-col h-full">
      <div className="p-2 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800">
        Layers
      </div>
      <div className="flex-1 overflow-y-auto p-1 space-y-1">
        {frames.map((frame) => (
          <div key={frame.id}>
            <button
              onClick={() => selectFrame(frame.id)}
              className={`w-full flex items-center gap-2 px-2 py-1.5 rounded text-xs transition-colors
                ${selectedFrameId === frame.id ? "bg-blue-600/20 text-blue-300" : "hover:bg-zinc-800 text-zinc-400"}`}
            >
              <span className={`${selectedFrameId === frame.id ? "text-blue-400" : "text-zinc-600"}`}>▸</span>
              {frame.name}
            </button>
            {selectedFrameId === frame.id && frame.layers.map((layer) => (
              <div
                key={layer.id}
                onClick={(e) => selectLayer(layer.id, e.metaKey || e.ctrlKey)}
                className={`flex items-center gap-2 px-2 py-1.5 ml-3 rounded cursor-pointer text-sm transition-colors
                  ${selectedLayerIds.has(layer.id) ? "bg-blue-600/20 text-blue-300" : "hover:bg-zinc-800 text-zinc-300"}`}
              >
                <button
                  onClick={(e) => { e.stopPropagation(); updateLayer(frame.id, layer.id, { visible: !layer.visible }); }}
                  className="w-4 text-center text-xs"
                >
                  {layer.visible ? "👁" : "—"}
                </button>
                <span className="flex-1 truncate">{layer.name}</span>
                <span className="text-[10px] text-zinc-600 uppercase">{layer.type}</span>
              </div>
            ))}
          </div>
        ))}
      </div>
      <div className="p-2 border-t border-zinc-800 flex gap-1">
        <button
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
          className="flex-1 px-2 py-1 text-xs bg-zinc-800 hover:bg-zinc-700 rounded"
        >
          + Add
        </button>
        <button
          onClick={() => {
            if (!selectedFrameId) return;
            selectedLayerIds.forEach((id) => removeLayer(selectedFrameId, id));
          }}
          className="px-2 py-1 text-xs bg-red-900/40 hover:bg-red-800/60 text-red-400 rounded"
          disabled={selectedLayerIds.size === 0}
        >
          Del
        </button>
      </div>
    </div>
  );
}
