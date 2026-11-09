import { useState, useRef } from "react";
import { useEditorStore } from "../stores/useEditorStore";

export default function TopBar() {
  const project = useEditorStore((s) => s.project);
  const zoom = useEditorStore((s) => s.zoom);
  const setZoom = useEditorStore((s) => s.setZoom);
  const setPan = useEditorStore((s) => s.setPan);
  const activeTool = useEditorStore((s) => s.activeTool);
  const exportSvg = useEditorStore((s) => s.exportSvg);
  const importSvg = useEditorStore((s) => s.importSvg);

  const [showExportModal, setShowExportModal] = useState(false);
  const [copied, setCopied] = useState(false);
  const fileInputRef = useRef<HTMLInputElement>(null);

  const svgContent = exportSvg();

  const handleCopy = async () => {
    try {
      await navigator.clipboard.writeText(svgContent);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    } catch (e) {
      console.error(e);
    }
  };

  const handleDownload = () => {
    const blob = new Blob([svgContent], { type: "image/svg+xml;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${(project?.name || "vector-graphic").toLowerCase().replace(/\s+/g, "-")}.svg`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (event) => {
      const text = event.target?.result as string;
      if (text) importSvg(text);
    };
    reader.readAsText(file);
    e.target.value = "";
  };

  return (
    <header role="banner" className="flex flex-wrap items-center justify-between gap-2 min-h-10 px-4 py-1 bg-zinc-900 border-b border-zinc-800 shrink-0 z-20">
      <div className="flex items-center gap-3">
        <span className="font-semibold text-sm text-zinc-200">Nexus-Graphic</span>
        {project && (
          <span className="text-sm text-zinc-400">{project.name}</span>
        )}
      </div>

      <div className="flex items-center gap-2">
        {/* Hidden File Input for SVG Import */}
        <input
          ref={fileInputRef}
          type="file"
          accept=".svg,image/svg+xml"
          onChange={handleFileChange}
          className="hidden"
        />

        <button
          type="button"
          onClick={() => fileInputRef.current?.click()}
          className="text-xs px-2.5 py-1.5 bg-zinc-800 hover:bg-zinc-700 text-zinc-300 border border-zinc-700 rounded-lg font-medium transition-colors"
        >
          Import SVG
        </button>

        <button
          type="button"
          onClick={() => setShowExportModal(true)}
          className="text-xs px-2.5 py-1.5 bg-indigo-600/30 hover:bg-indigo-600/50 text-indigo-300 border border-indigo-500/40 rounded-lg font-medium transition-colors"
        >
          Export SVG
        </button>

        <div className="flex items-center gap-2 text-xs text-zinc-500 ml-2">
          <span>Tool: {activeTool}</span>
          <span className="text-zinc-700">|</span>
          <button type="button" aria-label="Zoom out" onClick={() => setZoom(zoom - 0.1)} className="min-h-[38px] min-w-[38px] rounded hover:text-zinc-200 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent">−</button>
          <span>{Math.round(zoom * 100)}%</span>
          <button type="button" aria-label="Zoom in" onClick={() => setZoom(zoom + 0.1)} className="min-h-[38px] min-w-[38px] rounded hover:text-zinc-200 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent">+</button>
          <button type="button" aria-label="Fit canvas" onClick={() => { setZoom(1); setPan({ x: 0, y: 0 }); }} className="min-h-[38px] rounded px-2 hover:text-zinc-200 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ml-1">
            Fit
          </button>
        </div>
      </div>

      {/* SVG Export Modal */}
      {showExportModal && (
        <div className="fixed inset-0 bg-black/60 backdrop-blur-sm z-50 flex items-center justify-center p-4">
          <div className="bg-zinc-900 border border-zinc-800 rounded-2xl w-full max-w-lg p-5 shadow-2xl flex flex-col gap-4 animate-in fade-in zoom-in-95 duration-150">
            <div className="flex items-center justify-between pb-3 border-b border-zinc-800">
              <h2 className="text-sm font-semibold text-white">
                Export Vector SVG
              </h2>
              <button
                type="button"
                onClick={() => setShowExportModal(false)}
                className="text-zinc-500 hover:text-zinc-200 text-sm font-bold"
              >
                ✕
              </button>
            </div>

            <pre className="bg-zinc-950 border border-zinc-800 rounded-xl p-3 text-xs font-mono text-indigo-300 max-h-64 overflow-y-auto overflow-x-auto shadow-inner leading-relaxed">
              {svgContent}
            </pre>

            <div className="flex justify-end gap-2 pt-2 border-t border-zinc-800">
              <button
                type="button"
                onClick={handleCopy}
                className="px-3 py-1.5 text-xs bg-zinc-800 hover:bg-zinc-700 text-zinc-200 border border-zinc-700 rounded-lg transition-colors"
              >
                {copied ? "Copied!" : "Copy SVG Code"}
              </button>
              <button
                type="button"
                onClick={handleDownload}
                className="px-3 py-1.5 text-xs bg-indigo-600 hover:bg-indigo-500 text-white font-medium rounded-lg transition-colors"
              >
                Download SVG
              </button>
            </div>
          </div>
        </div>
      )}
    </header>
  );
}
