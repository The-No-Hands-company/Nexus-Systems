import { useEditorStore } from "../stores/useEditorStore";

// Keys follow the whiteboard convention (Excalidraw/tldraw): O is the ellipse,
// E is the eraser. Both previously claimed "E", and the canvas bound "e" to
// ellipse, so the eraser's advertised shortcut silently did nothing.
//
// "fill" and "zoom" are deliberately absent: neither was ever a real tool —
// there is no fill implementation, and zoom is driven by the top bar and
// Ctrl +/-/0. Listing them as selectable tools advertised behaviour the editor
// does not have.
export const tools = [
  { id: "select", label: "Select", key: "V", icon: "⇱" },
  { id: "hand", label: "Hand", key: "H", icon: "✋" },
  { id: "pen", label: "Pen", key: "P", icon: "✎" },
  { id: "rectangle", label: "Rectangle", key: "R", icon: "▭" },
  { id: "ellipse", label: "Ellipse", key: "O", icon: "○" },
  { id: "line", label: "Line", key: "L", icon: "╱" },
  { id: "arrow", label: "Arrow", key: "A", icon: "→" },
  { id: "text", label: "Text", key: "T", icon: "T" },
  { id: "sticky", label: "Sticky", key: "S", icon: "☐" },
  { id: "eraser", label: "Eraser", key: "E", icon: "⌫" },
];

export default function Toolbar() {
  const activeTool = useEditorStore((s) => s.activeTool);
  const setActiveTool = useEditorStore((s) => s.setActiveTool);

  return (
    <nav role="toolbar" aria-label="Drawing tools" aria-orientation="vertical" className="flex flex-col gap-0.5 p-1.5 bg-zinc-900 border-r border-zinc-800 w-12 sm:w-14 shrink-0 overflow-y-auto">
      {tools.map((tool) => (
        <button
          key={tool.id}
          onClick={() => setActiveTool(tool.id)}
          title={`${tool.label} (${tool.key})`}
          aria-label={`${tool.label} — press ${tool.key}`}
          aria-pressed={activeTool === tool.id}
          className={`w-9 h-9 sm:w-10 sm:h-10 flex items-center justify-center rounded-lg text-sm transition-colors focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-500
            ${activeTool === tool.id ? "bg-blue-600 text-white" : "text-zinc-400 hover:bg-zinc-800 hover:text-zinc-200"}`}
        >
          <span aria-hidden="true">{tool.icon}</span>
        </button>
      ))}
      <div className="mt-auto pt-2 border-t border-zinc-800">
        <div className="text-[10px] text-zinc-600 text-center leading-tight" aria-hidden="true">v0.1</div>
      </div>
    </nav>
  );
}
