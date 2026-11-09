import { useEditorStore, type TypographyStyle } from "../stores/useEditorStore";

const FONT_FAMILIES = [
  "Inter",
  "Roboto",
  "Space Grotesk",
  "JetBrains Mono",
  "Playfair Display",
  "system-ui",
];

export default function PropertiesPanel() {
  const frames = useEditorStore((s) => s.frames);
  const selectedFrameId = useEditorStore((s) => s.selectedFrameId);
  const selectedLayerIds = useEditorStore((s) => s.selectedLayerIds);
  const updateLayer = useEditorStore((s) => s.updateLayer);
  const updateFrame = useEditorStore((s) => s.updateFrame);
  const createMasterComponent = useEditorStore((s) => s.createMasterComponent);
  const createInstance = useEditorStore((s) => s.createInstance);
  const detachInstance = useEditorStore((s) => s.detachInstance);
  const updateAutoLayout = useEditorStore((s) => s.updateAutoLayout);

  const selectedFrame = frames.find((f) => f.id === selectedFrameId);

  if (!selectedFrame) {
    return (
      <div className="p-4 text-sm text-zinc-500 flex items-center justify-center h-full">
        No frame selected
      </div>
    );
  }

  const layer = selectedLayerIds.size > 0
    ? selectedFrame.layers.find((l) => selectedLayerIds.has(l.id))
    : null;

  return (
    <aside aria-label="Properties" className="flex flex-col h-full bg-zinc-900 border-l border-zinc-800">
      <div className="p-3 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800 flex items-center justify-between">
        <span>{layer ? "Layer Properties" : "Frame Properties"}</span>
        {layer?.isMasterComponent && (
          <span className="text-[10px] bg-purple-500/20 text-purple-300 border border-purple-500/30 px-1.5 py-0.5 rounded font-mono font-normal">
            ❖ Master
          </span>
        )}
        {layer?.masterComponentId && (
          <span className="text-[10px] bg-indigo-500/20 text-indigo-300 border border-indigo-500/30 px-1.5 py-0.5 rounded font-mono font-normal">
            ◇ Instance
          </span>
        )}
      </div>

      <div className="flex-1 overflow-y-auto p-3 space-y-4 text-sm">
        {layer ? (
          <>
            {/* General Layer Info */}
            <div className="space-y-2">
              <PropertyRow label="Name">
                <input
                  aria-label="Layer name"
                  value={layer.name}
                  onChange={(e) => updateLayer(selectedFrame.id, layer.id, { name: e.target.value })}
                  className="flex-1 bg-zinc-800 px-2 py-1 rounded text-sm border border-zinc-700 focus:border-accent outline-none"
                />
              </PropertyRow>

              <PropertyRow label="Type">
                <span className="text-xs uppercase bg-zinc-800 px-2 py-0.5 rounded text-zinc-400 font-mono">
                  {layer.type}
                </span>
              </PropertyRow>

              <PropertyRow label="Opacity">
                <input
                  aria-label="Layer opacity"
                  type="range" min="0" max="1" step="0.01"
                  value={layer.opacity}
                  onChange={(e) => updateLayer(selectedFrame.id, layer.id, { opacity: parseFloat(e.target.value) })}
                  className="flex-1"
                />
                <span className="w-8 text-right text-zinc-400 text-xs">{Math.round(layer.opacity * 100)}%</span>
              </PropertyRow>

              <div className="flex items-center gap-4 pt-1">
                <label className="flex items-center gap-1.5 text-xs text-zinc-400 cursor-pointer">
                  <input
                    aria-label="Layer visibility"
                    type="checkbox"
                    checked={layer.visible}
                    onChange={(e) => updateLayer(selectedFrame.id, layer.id, { visible: e.target.checked })}
                    className="rounded border-zinc-700 text-accent"
                  />
                  <span>Visible</span>
                </label>
                <label className="flex items-center gap-1.5 text-xs text-zinc-400 cursor-pointer">
                  <input
                    aria-label="Layer lock"
                    type="checkbox"
                    checked={layer.locked}
                    onChange={(e) => updateLayer(selectedFrame.id, layer.id, { locked: e.target.checked })}
                    className="rounded border-zinc-700 text-accent"
                  />
                  <span>Locked</span>
                </label>
              </div>
            </div>

            {/* Component Master & Instance System */}
            <div className="pt-2 border-t border-zinc-800 space-y-2">
              <div className="text-[11px] font-semibold uppercase tracking-wider text-zinc-400">
                Component System
              </div>
              {layer.isMasterComponent ? (
                <div className="space-y-1.5">
                  <div className="text-xs text-purple-300 bg-purple-950/40 border border-purple-500/30 p-2 rounded-lg leading-relaxed">
                    ❖ This is a Master Component. Edits will propagate to linked instances.
                  </div>
                  <button
                    type="button"
                    onClick={() => createInstance(layer.id, selectedFrame.id)}
                    className="w-full text-xs py-1.5 bg-purple-600 hover:bg-purple-500 text-white font-medium rounded-lg transition-colors"
                  >
                    + Create Instance
                  </button>
                </div>
              ) : layer.masterComponentId ? (
                <div className="space-y-1.5">
                  <div className="text-xs text-indigo-300 bg-indigo-950/40 border border-indigo-500/30 p-2 rounded-lg leading-relaxed">
                    ◇ Linked Component Instance. Inherits updates from master.
                  </div>
                  <button
                    type="button"
                    onClick={() => detachInstance(selectedFrame.id, layer.id)}
                    className="w-full text-xs py-1.5 bg-zinc-800 hover:bg-zinc-700 text-zinc-300 rounded-lg transition-colors border border-zinc-700"
                  >
                    Detach Instance
                  </button>
                </div>
              ) : (
                <button
                  type="button"
                  onClick={() => createMasterComponent(selectedFrame.id, layer.id)}
                  className="w-full text-xs py-1.5 bg-purple-600/30 hover:bg-purple-600/50 text-purple-200 border border-purple-500/40 font-medium rounded-lg transition-colors"
                >
                  ❖ Create Master Component
                </button>
              )}
            </div>

            {/* Transform Properties */}
            <div className="pt-2 border-t border-zinc-800 space-y-2">
              <div className="text-[11px] font-semibold uppercase tracking-wider text-zinc-400">
                Transform
              </div>
              <div className="grid grid-cols-2 gap-2">
                <PropertyRow label="X">
                  <input
                    type="number"
                    value={(layer.data as any).x ?? 0}
                    onChange={(e) => updateLayer(selectedFrame.id, layer.id, {
                      data: { ...layer.data, x: parseFloat(e.target.value) || 0 }
                    })}
                    className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 text-xs"
                  />
                </PropertyRow>
                <PropertyRow label="Y">
                  <input
                    type="number"
                    value={(layer.data as any).y ?? 0}
                    onChange={(e) => updateLayer(selectedFrame.id, layer.id, {
                      data: { ...layer.data, y: parseFloat(e.target.value) || 0 }
                    })}
                    className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 text-xs"
                  />
                </PropertyRow>
                <PropertyRow label="W">
                  <input
                    type="number"
                    value={(layer.data as any).width ?? 100}
                    onChange={(e) => updateLayer(selectedFrame.id, layer.id, {
                      data: { ...layer.data, width: parseFloat(e.target.value) || 0 }
                    })}
                    className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 text-xs"
                  />
                </PropertyRow>
                <PropertyRow label="H">
                  <input
                    type="number"
                    value={(layer.data as any).height ?? 100}
                    onChange={(e) => updateLayer(selectedFrame.id, layer.id, {
                      data: { ...layer.data, height: parseFloat(e.target.value) || 0 }
                    })}
                    className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 text-xs"
                  />
                </PropertyRow>
              </div>
            </div>

            {/* Fill & Stroke */}
            <div className="pt-2 border-t border-zinc-800 space-y-2">
              <div className="text-[11px] font-semibold uppercase tracking-wider text-zinc-400">
                Fill & Appearance
              </div>
              <PropertyRow label="Fill">
                <input
                  type="color"
                  value={(layer.data as any).fill ?? "#3b82f6"}
                  onChange={(e) => updateLayer(selectedFrame.id, layer.id, {
                    data: { ...layer.data, fill: e.target.value }
                  })}
                  className="w-8 h-7 bg-transparent border-0 cursor-pointer"
                />
                <span className="text-xs font-mono text-zinc-400">{(layer.data as any).fill ?? "#3b82f6"}</span>
              </PropertyRow>
            </div>

            {/* Typography Engine (for text layers or custom labels) */}
            {(layer.type === "text" || (layer.data as any).text) && (
              <div className="pt-2 border-t border-zinc-800 space-y-2">
                <div className="text-[11px] font-semibold uppercase tracking-wider text-zinc-400">
                  Typography Engine
                </div>
                <PropertyRow label="Text">
                  <input
                    type="text"
                    value={(layer.data as any).text ?? ""}
                    onChange={(e) => updateLayer(selectedFrame.id, layer.id, {
                      data: { ...layer.data, text: e.target.value }
                    })}
                    className="flex-1 bg-zinc-800 px-2 py-1 rounded text-xs border border-zinc-700"
                  />
                </PropertyRow>

                <PropertyRow label="Font">
                  <select
                    value={layer.typography?.fontFamily || "Inter"}
                    onChange={(e) => updateLayer(selectedFrame.id, layer.id, {
                      typography: {
                        fontFamily: e.target.value,
                        fontSize: layer.typography?.fontSize || 16,
                        fontWeight: layer.typography?.fontWeight || 400,
                        lineHeight: layer.typography?.lineHeight || 1.4,
                        letterSpacing: layer.typography?.letterSpacing || 0,
                        textAlign: layer.typography?.textAlign || "left",
                      }
                    })}
                    className="flex-1 bg-zinc-800 px-2 py-1 rounded text-xs border border-zinc-700"
                  >
                    {FONT_FAMILIES.map((font) => (
                      <option key={font} value={font}>{font}</option>
                    ))}
                  </select>
                </PropertyRow>

                <div className="grid grid-cols-2 gap-2">
                  <PropertyRow label="Size">
                    <input
                      type="number"
                      value={layer.typography?.fontSize || 16}
                      onChange={(e) => updateLayer(selectedFrame.id, layer.id, {
                        typography: {
                          fontFamily: layer.typography?.fontFamily || "Inter",
                          fontSize: parseInt(e.target.value) || 16,
                          fontWeight: layer.typography?.fontWeight || 400,
                          lineHeight: layer.typography?.lineHeight || 1.4,
                          letterSpacing: layer.typography?.letterSpacing || 0,
                          textAlign: layer.typography?.textAlign || "left",
                        }
                      })}
                      className="w-full bg-zinc-800 px-2 py-1 rounded text-xs border border-zinc-700"
                    />
                  </PropertyRow>

                  <PropertyRow label="Weight">
                    <select
                      value={layer.typography?.fontWeight || 400}
                      onChange={(e) => updateLayer(selectedFrame.id, layer.id, {
                        typography: {
                          fontFamily: layer.typography?.fontFamily || "Inter",
                          fontSize: layer.typography?.fontSize || 16,
                          fontWeight: parseInt(e.target.value) || 400,
                          lineHeight: layer.typography?.lineHeight || 1.4,
                          letterSpacing: layer.typography?.letterSpacing || 0,
                          textAlign: layer.typography?.textAlign || "left",
                        }
                      })}
                      className="w-full bg-zinc-800 px-2 py-1 rounded text-xs border border-zinc-700"
                    >
                      <option value="300">Light (300)</option>
                      <option value="400">Regular (400)</option>
                      <option value="500">Medium (500)</option>
                      <option value="600">Semibold (600)</option>
                      <option value="700">Bold (700)</option>
                    </select>
                  </PropertyRow>
                </div>
              </div>
            )}
          </>
        ) : (
          <>
            {/* Frame Properties */}
            <div className="space-y-2">
              <PropertyRow label="Name">
                <input
                  aria-label="Frame name"
                  value={selectedFrame.name}
                  onChange={(e) => updateFrame(selectedFrame.id, { name: e.target.value })}
                  className="flex-1 bg-zinc-800 px-2 py-1 rounded text-sm border border-zinc-700 focus:border-accent outline-none"
                />
              </PropertyRow>
              <div className="grid grid-cols-2 gap-2">
                <PropertyRow label="W">
                  <input
                    type="number"
                    value={selectedFrame.width}
                    onChange={(e) => updateFrame(selectedFrame.id, { width: parseInt(e.target.value) || 0 })}
                    className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 text-xs"
                  />
                </PropertyRow>
                <PropertyRow label="H">
                  <input
                    type="number"
                    value={selectedFrame.height}
                    onChange={(e) => updateFrame(selectedFrame.id, { height: parseInt(e.target.value) || 0 })}
                    className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 text-xs"
                  />
                </PropertyRow>
              </div>
              <PropertyRow label="Bg">
                <input
                  type="color"
                  value={selectedFrame.background}
                  onChange={(e) => updateFrame(selectedFrame.id, { background: e.target.value })}
                  className="w-8 h-7 bg-transparent border-0 cursor-pointer"
                />
                <span className="text-xs font-mono text-zinc-400">{selectedFrame.background}</span>
              </PropertyRow>
            </div>

            {/* Flex Auto-Layout Engine */}
            <div className="pt-2 border-t border-zinc-800 space-y-2.5">
              <div className="flex items-center justify-between">
                <span className="text-[11px] font-semibold uppercase tracking-wider text-zinc-400">
                  Flex Auto-Layout
                </span>
                <input
                  type="checkbox"
                  checked={selectedFrame.autoLayout?.enabled ?? false}
                  onChange={(e) => updateAutoLayout(selectedFrame.id, { enabled: e.target.checked })}
                  className="rounded border-zinc-700 text-accent cursor-pointer"
                />
              </div>

              {selectedFrame.autoLayout?.enabled && (
                <div className="space-y-2 pt-1 animate-in fade-in duration-150">
                  <PropertyRow label="Direction">
                    <div className="flex gap-1 flex-1">
                      <button
                        type="button"
                        onClick={() => updateAutoLayout(selectedFrame.id, { direction: "row" })}
                        className={`flex-1 text-xs py-1 rounded border transition-colors ${
                          selectedFrame.autoLayout.direction === "row"
                            ? "bg-accent/20 border-accent text-accent font-semibold"
                            : "bg-zinc-800 border-zinc-700 text-zinc-400"
                        }`}
                      >
                        Horizontal →
                      </button>
                      <button
                        type="button"
                        onClick={() => updateAutoLayout(selectedFrame.id, { direction: "column" })}
                        className={`flex-1 text-xs py-1 rounded border transition-colors ${
                          selectedFrame.autoLayout.direction === "column"
                            ? "bg-accent/20 border-accent text-accent font-semibold"
                            : "bg-zinc-800 border-zinc-700 text-zinc-400"
                        }`}
                      >
                        Vertical ↓
                      </button>
                    </div>
                  </PropertyRow>

                  <div className="grid grid-cols-2 gap-2">
                    <PropertyRow label="Gap">
                      <input
                        type="number"
                        value={selectedFrame.autoLayout.gap ?? 16}
                        onChange={(e) => updateAutoLayout(selectedFrame.id, { gap: parseInt(e.target.value) || 0 })}
                        className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 text-xs"
                      />
                    </PropertyRow>
                    <PropertyRow label="Pad">
                      <input
                        type="number"
                        value={selectedFrame.autoLayout.padding ?? 24}
                        onChange={(e) => updateAutoLayout(selectedFrame.id, { padding: parseInt(e.target.value) || 0 })}
                        className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 text-xs"
                      />
                    </PropertyRow>
                  </div>
                </div>
              )}
            </div>
          </>
        )}
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
