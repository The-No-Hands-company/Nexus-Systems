import { useEditorStore } from "../stores/useEditorStore";

export default function PropertiesPanel() {
  const project = useEditorStore((s) => s.project);
  const selectedLayerIds = useEditorStore((s) => s.selectedLayerIds);
  const updateLayer = useEditorStore((s) => s.updateLayer);
  const selectedNodeIndex = useEditorStore((s) => s.selectedNodeIndex);
  const selectNode = useEditorStore((s) => s.selectNode);
  const updateNode = useEditorStore((s) => s.updateNode);
  const addNode = useEditorStore((s) => s.addNode);
  const removeNode = useEditorStore((s) => s.removeNode);
  const updateGradient = useEditorStore((s) => s.updateGradient);

  if (!project || selectedLayerIds.size === 0) {
    return (
      <div className="p-4 text-sm text-zinc-500 flex items-center justify-center h-full">
        {!project ? "No project" : "No layer selected"}
      </div>
    );
  }

  const layer = project.layers.find((l) => selectedLayerIds.has(l.id));
  if (!layer) return null;

  const nodes = layer.nodes || [];
  const activeNode = selectedNodeIndex !== null && nodes[selectedNodeIndex] ? nodes[selectedNodeIndex] : null;

  return (
    <aside aria-label="Properties" className="flex flex-col h-full bg-zinc-900 border-l border-zinc-800">
      <div className="p-3 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800 flex items-center justify-between">
        <span>Vector Properties</span>
        <span className="text-[10px] bg-zinc-800 text-zinc-400 px-1.5 py-0.5 rounded font-mono uppercase">
          {layer.type}
        </span>
      </div>

      <div className="flex-1 overflow-y-auto p-3 space-y-4 text-sm">
        {/* Layer Identity */}
        <div className="space-y-2">
          <PropertyRow label="Name">
            <input
              aria-label="Layer name"
              value={layer.name}
              onChange={(e) => updateLayer(layer.id, { name: e.target.value })}
              className="flex-1 bg-zinc-800 px-2 py-1 rounded text-sm border border-zinc-700 focus:border-accent outline-none"
            />
          </PropertyRow>
          <PropertyRow label="Opacity">
            <input
              aria-label="Layer opacity"
              type="range" min="0" max="1" step="0.01"
              value={layer.opacity}
              onChange={(e) => updateLayer(layer.id, { opacity: parseFloat(e.target.value) })}
              className="flex-1"
            />
            <span className="w-8 text-right text-zinc-400 text-xs">{Math.round(layer.opacity * 100)}%</span>
          </PropertyRow>
        </div>

        {/* Sub-path Node Editing Mode */}
        <div className="pt-2 border-t border-zinc-800 space-y-2.5">
          <div className="flex items-center justify-between">
            <span className="text-[11px] font-semibold uppercase tracking-wider text-zinc-400">
              Sub-path Node Editing
            </span>
            <span className="text-[10px] text-zinc-500 font-mono">
              {nodes.length} nodes
            </span>
          </div>

          {nodes.length === 0 ? (
            <button
              type="button"
              onClick={() => {
                const initialNodes = [
                  { id: crypto.randomUUID(), x: 40, y: 40, type: "corner" as const },
                  { id: crypto.randomUUID(), x: 160, y: 40, type: "smooth" as const },
                  { id: crypto.randomUUID(), x: 100, y: 160, type: "corner" as const },
                ];
                updateLayer(layer.id, { type: "path", nodes: initialNodes });
                selectNode(0);
              }}
              className="w-full text-xs py-1.5 bg-indigo-600/30 hover:bg-indigo-600/50 text-indigo-300 border border-indigo-500/40 rounded-lg transition-colors font-medium"
            >
              Convert to Editable Path Nodes
            </button>
          ) : (
            <div className="space-y-2">
              <div className="flex items-center gap-1 overflow-x-auto pb-1">
                {nodes.map((n, idx) => (
                  <button
                    key={n.id}
                    type="button"
                    onClick={() => selectNode(idx)}
                    className={`px-2.5 py-1 text-xs rounded-md border font-mono transition-colors ${
                      selectedNodeIndex === idx
                        ? "bg-accent/20 border-accent text-accent font-bold"
                        : "bg-zinc-800 border-zinc-700 text-zinc-400 hover:text-white"
                    }`}
                  >
                    #{idx + 1}
                  </button>
                ))}
                <button
                  type="button"
                  onClick={() => {
                    const last = nodes[nodes.length - 1];
                    addNode(layer.id, {
                      id: crypto.randomUUID(),
                      x: last ? last.x + 30 : 50,
                      y: last ? last.y + 30 : 50,
                      type: "corner",
                    });
                    selectNode(nodes.length);
                  }}
                  className="px-2 py-1 text-xs bg-zinc-800 hover:bg-zinc-700 text-zinc-300 rounded border border-zinc-700"
                  title="Add node"
                >
                  +
                </button>
              </div>

              {activeNode !== null && selectedNodeIndex !== null && (
                <div className="p-2.5 bg-zinc-800/60 border border-zinc-700/60 rounded-xl space-y-2">
                  <div className="flex items-center justify-between text-xs font-semibold text-zinc-300">
                    <span>Node #{selectedNodeIndex + 1} Properties</span>
                    <button
                      type="button"
                      onClick={() => removeNode(layer.id, selectedNodeIndex)}
                      className="text-[10px] text-red-400 hover:underline"
                    >
                      Delete
                    </button>
                  </div>

                  <div className="grid grid-cols-2 gap-2">
                    <PropertyRow label="X">
                      <input
                        type="number"
                        value={activeNode.x}
                        onChange={(e) => updateNode(layer.id, selectedNodeIndex, { x: parseFloat(e.target.value) || 0 })}
                        className="w-full bg-zinc-900 px-2 py-1 rounded border border-zinc-700 text-xs"
                      />
                    </PropertyRow>
                    <PropertyRow label="Y">
                      <input
                        type="number"
                        value={activeNode.y}
                        onChange={(e) => updateNode(layer.id, selectedNodeIndex, { y: parseFloat(e.target.value) || 0 })}
                        className="w-full bg-zinc-900 px-2 py-1 rounded border border-zinc-700 text-xs"
                      />
                    </PropertyRow>
                  </div>

                  <PropertyRow label="Vertex">
                    <div className="flex gap-1 flex-1">
                      <button
                        type="button"
                        onClick={() => updateNode(layer.id, selectedNodeIndex, { type: "corner" })}
                        className={`flex-1 text-xs py-1 rounded border transition-colors ${
                          activeNode.type === "corner"
                            ? "bg-accent/20 border-accent text-accent font-semibold"
                            : "bg-zinc-900 border-zinc-700 text-zinc-400"
                        }`}
                      >
                        Corner ⬦
                      </button>
                      <button
                        type="button"
                        onClick={() => updateNode(layer.id, selectedNodeIndex, { type: "smooth" })}
                        className={`flex-1 text-xs py-1 rounded border transition-colors ${
                          activeNode.type === "smooth"
                            ? "bg-accent/20 border-accent text-accent font-semibold"
                            : "bg-zinc-900 border-zinc-700 text-zinc-400"
                        }`}
                      >
                        Smooth ◯
                      </button>
                    </div>
                  </PropertyRow>
                </div>
              )}
            </div>
          )}
        </div>

        {/* Gradient & Color Styling Engine */}
        <div className="pt-2 border-t border-zinc-800 space-y-2.5">
          <div className="flex items-center justify-between">
            <span className="text-[11px] font-semibold uppercase tracking-wider text-zinc-400">
              Fill & Gradients
            </span>
            <div className="flex gap-1 text-[10px]">
              <button
                type="button"
                onClick={() => updateGradient(layer.id, null)}
                className={`px-1.5 py-0.5 rounded border ${
                  !layer.gradient
                    ? "bg-accent/20 border-accent text-accent font-bold"
                    : "bg-zinc-800 border-zinc-700 text-zinc-400"
                }`}
              >
                Solid
              </button>
              <button
                type="button"
                onClick={() => updateGradient(layer.id, { type: "linear" })}
                className={`px-1.5 py-0.5 rounded border ${
                  layer.gradient?.type === "linear"
                    ? "bg-accent/20 border-accent text-accent font-bold"
                    : "bg-zinc-800 border-zinc-700 text-zinc-400"
                }`}
              >
                Linear
              </button>
              <button
                type="button"
                onClick={() => updateGradient(layer.id, { type: "radial" })}
                className={`px-1.5 py-0.5 rounded border ${
                  layer.gradient?.type === "radial"
                    ? "bg-accent/20 border-accent text-accent font-bold"
                    : "bg-zinc-800 border-zinc-700 text-zinc-400"
                }`}
              >
                Radial
              </button>
            </div>
          </div>

          {!layer.gradient ? (
            <PropertyRow label="Color">
              <input
                type="color"
                value={(layer.data as any).fill ?? "#3b82f6"}
                onChange={(e) => updateLayer(layer.id, {
                  data: { ...layer.data, fill: e.target.value }
                })}
                className="w-8 h-7 bg-transparent border-0 cursor-pointer"
              />
              <span className="text-xs font-mono text-zinc-400">{(layer.data as any).fill ?? "#3b82f6"}</span>
            </PropertyRow>
          ) : (
            <div className="p-2.5 bg-zinc-800/60 border border-zinc-700/60 rounded-xl space-y-2">
              {layer.gradient.type === "linear" && (
                <PropertyRow label="Angle">
                  <input
                    type="range"
                    min="0"
                    max="360"
                    value={layer.gradient.angle || 0}
                    onChange={(e) => updateGradient(layer.id, { angle: parseInt(e.target.value) || 0 })}
                    className="flex-1"
                  />
                  <span className="w-8 text-right text-zinc-400 text-xs font-mono">
                    {layer.gradient.angle || 0}°
                  </span>
                </PropertyRow>
              )}

              <div className="space-y-1.5 pt-1">
                <span className="text-[10px] uppercase font-semibold text-zinc-400 tracking-wider">
                  Color Stops
                </span>
                {layer.gradient.stops.map((stop, sIdx) => (
                  <div key={sIdx} className="flex items-center gap-2">
                    <input
                      type="color"
                      value={stop.color}
                      onChange={(e) => {
                        const newStops = layer.gradient?.stops.map((st, i) =>
                          i === sIdx ? { ...st, color: e.target.value } : st
                        );
                        updateGradient(layer.id, { stops: newStops });
                      }}
                      className="w-6 h-6 bg-transparent border-0 cursor-pointer"
                    />
                    <input
                      type="range"
                      min="0"
                      max="100"
                      value={stop.offset}
                      onChange={(e) => {
                        const newStops = layer.gradient?.stops.map((st, i) =>
                          i === sIdx ? { ...st, offset: parseInt(e.target.value) || 0 } : st
                        );
                        updateGradient(layer.id, { stops: newStops });
                      }}
                      className="flex-1"
                    />
                    <span className="text-xs font-mono text-zinc-400 w-8 text-right">
                      {stop.offset}%
                    </span>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* Stroke weight & color */}
          <div className="space-y-2 pt-1">
            <PropertyRow label="Stroke">
              <input
                type="color"
                value={(layer.data as any).stroke ?? "#000000"}
                onChange={(e) => updateLayer(layer.id, {
                  data: { ...layer.data, stroke: e.target.value }
                })}
                className="w-8 h-7 bg-transparent border-0 cursor-pointer"
              />
              <span className="text-xs font-mono text-zinc-400">{(layer.data as any).stroke ?? "none"}</span>
            </PropertyRow>
            <PropertyRow label="Weight">
              <input
                type="number"
                min="0"
                max="50"
                value={(layer.data as any).strokeWidth ?? 1}
                onChange={(e) => updateLayer(layer.id, {
                  data: { ...layer.data, strokeWidth: parseFloat(e.target.value) || 1 }
                })}
                className="w-20 bg-zinc-800 px-2 py-1 rounded border border-zinc-700 text-xs"
              />
            </PropertyRow>
          </div>
        </div>
      </div>
    </aside>
  );
}

function PropertyRow({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div className="flex items-center gap-2">
      <span className="text-xs text-zinc-500 w-16 shrink-0">{label}</span>
      {children}
    </div>
  );
}
