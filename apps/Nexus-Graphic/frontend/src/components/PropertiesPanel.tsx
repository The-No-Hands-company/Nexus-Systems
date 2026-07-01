import { useEditorStore } from "../stores/useEditorStore";

export default function PropertiesPanel() {
  const project = useEditorStore((s) => s.project);
  const selectedLayerIds = useEditorStore((s) => s.selectedLayerIds);
  const updateLayer = useEditorStore((s) => s.updateLayer);

  if (!project || selectedLayerIds.size === 0) {
    return (
      <div className="p-4 text-sm text-zinc-500 flex items-center justify-center h-full">
        {!project ? "No project" : "No layer selected"}
      </div>
    );
  }

  const layer = project.layers.find((l) => selectedLayerIds.has(l.id));
  if (!layer) return null;

  return (
    <div className="flex flex-col h-full">
      <div className="p-2 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800">
        Properties
      </div>
      <div className="flex-1 overflow-y-auto p-3 space-y-3 text-sm">
        <PropertyRow label="Name">
          <input
            value={layer.name}
            onChange={(e) => updateLayer(layer.id, { name: e.target.value })}
            className="flex-1 bg-zinc-800 px-2 py-1 rounded text-sm border border-zinc-700 focus:border-blue-500 outline-none"
          />
        </PropertyRow>
        <PropertyRow label="Opacity">
          <input
            type="range" min="0" max="1" step="0.01"
            value={layer.opacity}
            onChange={(e) => updateLayer(layer.id, { opacity: parseFloat(e.target.value) })}
            className="flex-1"
          />
          <span className="w-8 text-right text-zinc-400">{Math.round(layer.opacity * 100)}%</span>
        </PropertyRow>
        <PropertyRow label="Visible">
          <input
            type="checkbox"
            checked={layer.visible}
            onChange={(e) => updateLayer(layer.id, { visible: e.target.checked })}
          />
        </PropertyRow>
        <PropertyRow label="Locked">
          <input
            type="checkbox"
            checked={layer.locked}
            onChange={(e) => updateLayer(layer.id, { locked: e.target.checked })}
          />
        </PropertyRow>
        {layer.type === "shape" && (
          <>
            <PropertyRow label="X">
              <input type="number" value={(layer.data as any).x ?? 0}
                onChange={(e) => updateLayer(layer.id, {
                  data: { ...layer.data, x: parseFloat(e.target.value) }
                })}
                className="w-20 bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none text-sm" />
            </PropertyRow>
            <PropertyRow label="Y">
              <input type="number" value={(layer.data as any).y ?? 0}
                onChange={(e) => updateLayer(layer.id, {
                  data: { ...layer.data, y: parseFloat(e.target.value) }
                })}
                className="w-20 bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none text-sm" />
            </PropertyRow>
            <PropertyRow label="W">
              <input type="number" value={(layer.data as any).width ?? 100}
                onChange={(e) => updateLayer(layer.id, {
                  data: { ...layer.data, width: parseFloat(e.target.value) }
                })}
                className="w-20 bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none text-sm" />
            </PropertyRow>
            <PropertyRow label="H">
              <input type="number" value={(layer.data as any).height ?? 100}
                onChange={(e) => updateLayer(layer.id, {
                  data: { ...layer.data, height: parseFloat(e.target.value) }
                })}
                className="w-20 bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none text-sm" />
            </PropertyRow>
            <PropertyRow label="Fill">
              <input type="color" value={(layer.data as any).fill ?? "#3b82f6"}
                onChange={(e) => updateLayer(layer.id, {
                  data: { ...layer.data, fill: e.target.value }
                })}
                className="w-10 h-8 bg-transparent border-0 cursor-pointer" />
              <span className="text-xs text-zinc-500">{(layer.data as any).fill ?? "#3b82f6"}</span>
            </PropertyRow>
          </>
        )}
      </div>
    </div>
  );
}

function PropertyRow({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div className="flex items-center gap-2">
      <span className="text-xs text-zinc-500 w-12 shrink-0">{label}</span>
      {children}
    </div>
  );
}
