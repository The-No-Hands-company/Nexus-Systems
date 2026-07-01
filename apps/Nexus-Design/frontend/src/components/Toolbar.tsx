import { useEditorStore } from "../stores/useEditorStore";

const tools = [
  { id: "select", label: "Select", key: "V", icon: "⇱" },
  { id: "hand", label: "Hand", key: "H", icon: "✋" },
  { id: "frame", label: "Frame", key: "F", icon: "▭" },
  { id: "rectangle", label: "Rectangle", key: "R", icon: "▬" },
  { id: "ellipse", label: "Ellipse", key: "E", icon: "○" },
  { id: "text", label: "Text", key: "T", icon: "T" },
  { id: "component", label: "Component", key: "C", icon: "◆" },
  { id: "prototype", label: "Prototype", key: "P", icon: "▶" },
  { id: "zoom", label: "Zoom", key: "Z", icon: "🔍" },
];

export default function Toolbar() {
  const activeTool = useEditorStore((s) => s.activeTool);
  const setActiveTool = useEditorStore((s) => s.setActiveTool);

  return (
    <div className="flex flex-col gap-1 p-2 bg-zinc-900 border-r border-zinc-800 w-14 shrink-0">
      {tools.map((tool) => (
        <button
          key={tool.id}
          onClick={() => setActiveTool(tool.id as any)}
          title={`${tool.label} (${tool.key})`}
          className={`w-10 h-10 flex items-center justify-center rounded-lg text-sm transition-colors
            ${activeTool === tool.id ? "bg-blue-600 text-white" : "text-zinc-400 hover:bg-zinc-800 hover:text-zinc-200"}`}
        >
          {tool.icon}
        </button>
      ))}
      <div className="mt-auto pt-2 border-t border-zinc-800">
        <div className="text-[10px] text-zinc-600 text-center leading-tight">
          v0.1
        </div>
      </div>
    </div>
  );
}
