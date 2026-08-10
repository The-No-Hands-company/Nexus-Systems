import { useEditorStore } from "../stores/useEditorStore";
import type { ElementData, StyleMode } from "../stores/model";

export default function PropertiesPanel() {
  const elements = useEditorStore((s) => s.elements);
  const selectedElementIds = useEditorStore((s) => s.selectedElementIds);
  const updateElement = useEditorStore((s) => s.updateElement);
  const board = useEditorStore((s) => s.board);
  const updateBoard = useEditorStore((s) => s.updateBoard);

  const selected = elements.filter((el) => selectedElementIds.has(el.id));

  if (selected.length === 0) {
    const dsm = board?.defaultStyleMode ?? "clean";
    return (
      <div className="flex flex-col h-full">
        <div className="p-2 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800">
          Properties
        </div>
        <div className="flex-1 overflow-y-auto p-3 space-y-3 text-sm">
          <PropertyRow label="Style">
            <Segmented
              value={dsm}
              options={[
                { value: "clean", label: "Clean" },
                { value: "sketch", label: "Sketch" },
              ]}
              onChange={(v) => updateBoard({ defaultStyleMode: v as StyleMode })}
            />
          </PropertyRow>
          <PropertyRow label="Snap">
            <label className="flex items-center gap-2 text-xs text-zinc-400">
              <input
                type="checkbox"
                checked={board?.gridSnap ?? false}
                onChange={(e) => updateBoard({ gridSnap: e.target.checked })}
                className="accent-blue-500"
              />
              Grid snap
            </label>
          </PropertyRow>
          <div className="pt-2 text-xs text-zinc-600">No element selected</div>
        </div>
      </div>
    );
  }

  const first = selected[0];
  const s = first.style;
  const isTextLike = first.elementType === "text" || first.elementType === "sticky";
  const isBox = ["rectangle", "ellipse", "sticky", "image"].includes(first.elementType);

  const patchBy = (patch: Partial<ElementData["style"]>) => {
    selected.forEach((el) => updateElement(el.id, { style: { ...el.style, ...patch } }));
  };

  return (
    <div className="flex flex-col h-full">
      <div className="p-2 text-xs font-semibold text-zinc-400 uppercase tracking-wider border-b border-zinc-800">
        Properties
      </div>
      <div className="flex-1 overflow-y-auto p-3 space-y-3 text-sm">
        <PropertyRow label="X">
          <input type="number" value={Math.round((first.data.x ?? 0) * 100) / 100}
            onChange={(e) => selected.forEach((el) => updateElement(el.id, { data: { ...el.data, x: parseFloat(e.target.value) } }))}
            className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </PropertyRow>
        <PropertyRow label="Y">
          <input type="number" value={Math.round((first.data.y ?? 0) * 100) / 100}
            onChange={(e) => selected.forEach((el) => updateElement(el.id, { data: { ...el.data, y: parseFloat(e.target.value) } }))}
            className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </PropertyRow>
        {isBox && (
          <>
            <PropertyRow label="W">
              <input type="number" value={Math.round((first.data.width ?? 100) * 100) / 100}
                onChange={(e) => selected.forEach((el) => updateElement(el.id, { data: { ...el.data, width: Math.max(1, parseFloat(e.target.value)) } }))}
                className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
            </PropertyRow>
            <PropertyRow label="H">
              <input type="number" value={Math.round((first.data.height ?? 100) * 100) / 100}
                onChange={(e) => selected.forEach((el) => updateElement(el.id, { data: { ...el.data, height: Math.max(1, parseFloat(e.target.value)) } }))}
                className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
            </PropertyRow>
          </>
        )}
        <PropertyRow label="Fill">
          <input type="color" value={s.fill !== "none" ? s.fill : "#ffffff"}
            onChange={(e) => patchBy({ fill: e.target.value })}
            className="w-10 h-8 bg-transparent border-0 cursor-pointer" />
          <button onClick={() => patchBy({ fill: "none" })}
            className={`px-2 py-1 rounded text-xs border ${s.fill === "none" ? "border-blue-500 text-blue-400" : "border-zinc-700 text-zinc-400 hover:border-zinc-500"}`}>
            none
          </button>
        </PropertyRow>
        <PropertyRow label="Stroke">
          <input type="color" value={s.stroke}
            onChange={(e) => patchBy({ stroke: e.target.value })}
            className="w-10 h-8 bg-transparent border-0 cursor-pointer" />
        </PropertyRow>
        <PropertyRow label="Width">
          <input type="number" min="0" max="20" step="1" value={s.strokeWidth}
            onChange={(e) => patchBy({ strokeWidth: Math.max(0, parseFloat(e.target.value)) })}
            className="w-16 bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
        </PropertyRow>
        <PropertyRow label="Dashes">
          <Segmented
            value={s.strokeStyle}
            options={[
              { value: "solid", label: "—" },
              { value: "dashed", label: "═" },
              { value: "dotted", label: "·" },
            ]}
            onChange={(v) => patchBy({ strokeStyle: v as ElementData["style"]["strokeStyle"] })}
          />
        </PropertyRow>
        <PropertyRow label="Opacity">
          <input type="range" min="0" max="1" step="0.01" value={s.opacity}
            onChange={(e) => patchBy({ opacity: parseFloat(e.target.value) })}
            className="flex-1" />
          <span className="w-8 text-right text-zinc-400">{Math.round(s.opacity * 100)}%</span>
        </PropertyRow>
        {first.elementType === "rectangle" && (
          <PropertyRow label="Radius">
            <input type="range" min="0" max="50" step="1" value={s.radius}
              onChange={(e) => patchBy({ radius: parseFloat(e.target.value) })}
              className="flex-1" />
          </PropertyRow>
        )}
        <PropertyRow label="Mode">
          <Segmented
            value={s.styleMode ?? board?.defaultStyleMode ?? "clean"}
            options={[
              { value: "clean", label: "Clean" },
              { value: "sketch", label: "Sketch" },
            ]}
            onChange={(v) => patchBy({ styleMode: v as StyleMode })}
          />
        </PropertyRow>
        {isTextLike && (
          <>
            <PropertyRow label="Font">
              <select value={s.fontFamily} onChange={(e) => patchBy({ fontFamily: e.target.value })}
                className="w-full bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none text-sm">
                <option value="ui-sans-serif, system-ui">System</option>
                <option value="serif">Serif</option>
                <option value="monospace">Mono</option>
              </select>
            </PropertyRow>
            <PropertyRow label="Size">
              <input type="number" min="8" max="200" step="1" value={s.fontSize}
                onChange={(e) => patchBy({ fontSize: Math.max(8, parseFloat(e.target.value)) })}
                className="w-20 bg-zinc-800 px-2 py-1 rounded border border-zinc-700 focus:border-blue-500 outline-none" />
            </PropertyRow>
            <PropertyRow label="Align">
              <Segmented
                value={s.textAlign}
                options={[
                  { value: "left", label: "⇤" },
                  { value: "center", label: "↔" },
                  { value: "right", label: "⇥" },
                ]}
                onChange={(v) => patchBy({ textAlign: v as ElementData["style"]["textAlign"] })}
              />
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
      <span className="text-xs text-zinc-500 w-10 shrink-0">{label}</span>
      {children}
    </div>
  );
}

function Segmented({ value, options, onChange }: { value: string; options: { value: string; label: string }[]; onChange: (v: string) => void }) {
  return (
    <div className="flex rounded overflow-hidden border border-zinc-700">
      {options.map((o) => (
        <button
          key={o.value}
          onClick={() => onChange(o.value)}
          className={`px-2 py-1 text-xs ${value === o.value ? "bg-blue-600 text-white" : "bg-zinc-800 text-zinc-400 hover:bg-zinc-700"}`}
        >
          {o.label}
        </button>
      ))}
    </div>
  );
}