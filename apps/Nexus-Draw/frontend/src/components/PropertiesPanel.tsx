import { useEditorStore } from "../stores/useEditorStore";

export default function PropertiesPanel() {
  const elements = useEditorStore((s) => s.elements);
  const selectedElementIds = useEditorStore((s) => s.selectedElementIds);
  const updateElement = useEditorStore((s) => s.updateElement);

  if (selectedElementIds.size === 0) {
    return (
      <div className="p-4 text-sm text-zinc-500 flex items-center justify-center h-full">
        No element selected
      </div>
    );
  }

  const el = elements.find((e) => selectedElementIds.has(e.id));
  if (!el) return null;

  const data = el.data as Record<string, any>;
  const style = el.style as Record<string, any>;
  const t = el.transform;

  return (
    <div className="flex flex-col h-full">
      <div className="p-2 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800">
        Properties
      </div>
      <div className="flex-1 overflow-y-auto p-3 space-y-3 text-sm">
        <PropertyRow label="X">
          <input type="number" value={data.x ?? 0}
            onChange={(e) => updateElement(el.id, { data: { ...data, x: parseFloat(e.target.value) } })}
            className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </PropertyRow>
        <PropertyRow label="Y">
          <input type="number" value={data.y ?? 0}
            onChange={(e) => updateElement(el.id, { data: { ...data, y: parseFloat(e.target.value) } })}
            className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </PropertyRow>
        <PropertyRow label="W">
          <input type="number" value={data.width ?? 100}
            onChange={(e) => updateElement(el.id, { data: { ...data, width: parseFloat(e.target.value) } })}
            className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </PropertyRow>
        <PropertyRow label="H">
          <input type="number" value={data.height ?? 100}
            onChange={(e) => updateElement(el.id, { data: { ...data, height: parseFloat(e.target.value) } })}
            className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </PropertyRow>
        <PropertyRow label="Rot">
          <input type="number" value={Math.atan2(t.b, t.a) * (180 / Math.PI) || 0}
            onChange={(e) => {
              const angle = parseFloat(e.target.value) * (Math.PI / 180);
              const c = Math.cos(angle), s = Math.sin(angle);
              updateElement(el.id, { transform: { ...t, a: c, b: s, c: -s, d: c } });
            }}
            className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </PropertyRow>
        <PropertyRow label="Fill">
          <input type="color" value={style.fill ?? "#3b82f6"}
            onChange={(e) => updateElement(el.id, { style: { ...style, fill: e.target.value } })}
            className="w-10 h-8 bg-transparent border-0 cursor-pointer" />
          <span className="text-xs text-zinc-500">{style.fill ?? "#3b82f6"}</span>
        </PropertyRow>
        <PropertyRow label="Stroke">
          <input type="color" value={style.stroke ?? "#000000"}
            onChange={(e) => updateElement(el.id, { style: { ...style, stroke: e.target.value } })}
            className="w-10 h-8 bg-transparent border-0 cursor-pointer" />
        </PropertyRow>
        <PropertyRow label="Width">
          <input type="number" min="0" max="20" step="1" value={style.strokeWidth ?? 2}
            onChange={(e) => updateElement(el.id, { style: { ...style, strokeWidth: parseFloat(e.target.value) } })}
            className="w-16 bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </PropertyRow>
        <PropertyRow label="Opacity">
          <input type="range" min="0" max="1" step="0.01" value={style.opacity ?? 1}
            onChange={(e) => updateElement(el.id, { style: { ...style, opacity: parseFloat(e.target.value) } })}
            className="flex-1" />
          <span className="w-8 text-right text-zinc-400">{Math.round((style.opacity ?? 1) * 100)}%</span>
        </PropertyRow>
      </div>
    </div>
  );
}

function PropertyRow({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div className="flex items-center gap-2">
      <span className="text-xs text-zinc-500 w-10 shrink-0">{label}</span>
      {children}
    </div>
  );
}
