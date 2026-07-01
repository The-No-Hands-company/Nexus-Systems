import { useEditorStore } from "../stores/useEditorStore";

export default function TopBar() {
  const project = useEditorStore((s) => s.project);
  const frames = useEditorStore((s) => s.frames);
  const selectedFrameId = useEditorStore((s) => s.selectedFrameId);
  const selectFrame = useEditorStore((s) => s.selectFrame);
  const zoom = useEditorStore((s) => s.zoom);
  const setZoom = useEditorStore((s) => s.setZoom);
  const setPan = useEditorStore((s) => s.setPan);
  const activeTool = useEditorStore((s) => s.activeTool);

  return (
    <div className="flex items-center justify-between h-10 px-4 bg-zinc-900 border-b border-zinc-800 shrink-0">
      <div className="flex items-center gap-3">
        <span className="font-semibold text-sm text-zinc-200">Nexus-Design</span>
        {project && (
          <>
            <span className="text-sm text-zinc-400">{project.name}</span>
            <span className="text-[10px] uppercase text-zinc-600 bg-zinc-800 px-1.5 py-0.5 rounded">
              {project.projectType}
            </span>
          </>
        )}
      </div>
      <div className="flex items-center gap-3">
        {frames.length > 0 && (
          <select
            value={selectedFrameId ?? ""}
            onChange={(e) => selectFrame(e.target.value)}
            className="bg-zinc-800 text-xs text-zinc-300 px-2 py-1 rounded border border-zinc-700 outline-none"
          >
            {frames.map((f) => (
              <option key={f.id} value={f.id}>{f.name}</option>
            ))}
          </select>
        )}
        <div className="flex items-center gap-2 text-xs text-zinc-500">
          <span>Tool: {activeTool}</span>
          <span className="text-zinc-700">|</span>
          <button onClick={() => setZoom(zoom - 0.1)} className="hover:text-zinc-200">−</button>
          <span>{Math.round(zoom * 100)}%</span>
          <button onClick={() => setZoom(zoom + 0.1)} className="hover:text-zinc-200">+</button>
          <button onClick={() => { setZoom(1); setPan({ x: 0, y: 0 }); }} className="hover:text-zinc-200 ml-1">
            Fit
          </button>
        </div>
      </div>
    </div>
  );
}
