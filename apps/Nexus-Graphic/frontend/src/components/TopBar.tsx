import { useEditorStore } from "../stores/useEditorStore";

export default function TopBar() {
  const project = useEditorStore((s) => s.project);
  const zoom = useEditorStore((s) => s.zoom);
  const setZoom = useEditorStore((s) => s.setZoom);
  const setPan = useEditorStore((s) => s.setPan);
  const activeTool = useEditorStore((s) => s.activeTool);

  return (
    <header role="banner" className="flex flex-wrap items-center justify-between gap-2 min-h-10 px-4 py-1 bg-zinc-900 border-b border-zinc-800 shrink-0">
      <div className="flex items-center gap-3">
        <span className="font-semibold text-sm text-zinc-200">Nexus-Graphic</span>
        {project && (
          <span className="text-sm text-zinc-400">{project.name}</span>
        )}
      </div>
      <div className="flex items-center gap-2 text-xs text-zinc-500">
        <span>Tool: {activeTool}</span>
        <span className="text-zinc-700">|</span>
        <button type="button" aria-label="Zoom out" onClick={() => setZoom(zoom - 0.1)} className="min-h-[44px] min-w-[44px] rounded hover:text-zinc-200 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent">−</button>
        <span>{Math.round(zoom * 100)}%</span>
        <button type="button" aria-label="Zoom in" onClick={() => setZoom(zoom + 0.1)} className="min-h-[44px] min-w-[44px] rounded hover:text-zinc-200 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent">+</button>
        <button type="button" aria-label="Fit canvas" onClick={() => { setZoom(1); setPan({ x: 0, y: 0 }); }} className="min-h-[44px] rounded px-2 hover:text-zinc-200 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ml-1">
          Fit
        </button>
      </div>
    </header>
  );
}
