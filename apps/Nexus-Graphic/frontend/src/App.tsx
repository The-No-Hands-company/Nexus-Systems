import { useEffect, useState } from "react";
import Toolbar from "./components/Toolbar";
import TopBar from "./components/TopBar";
import Canvas from "./components/Canvas/Canvas";
import LayerPanel from "./components/LayerPanel";
import PropertiesPanel from "./components/PropertiesPanel";
import { useEditorStore } from "./stores/useEditorStore";

export default function App() {
  const [showPanels, setShowPanels] = useState(true);
  const sidebar = useEditorStore((s) => s.sidebar);
  const setSidebar = useEditorStore((s) => s.setSidebar);
  const project = useEditorStore((s) => s.project);

  useEffect(() => {
    document.title = project ? `${project.name} — Graphic` : "Nexus-Graphic";
  }, [project]);

  return (
    <div className="h-screen flex flex-col">
      <a
        href="#main-content"
        className="sr-only focus:not-sr-only focus:absolute focus:z-50 focus:left-2 focus:top-2 focus:rounded focus:bg-zinc-800 focus:px-4 focus:py-2 focus:text-zinc-100 focus:ring-2 focus:ring-accent focus:outline-none"
      >
        Skip to canvas
      </a>
      <TopBar />
      <div className="flex flex-1 overflow-hidden">
        <Toolbar />
        <main id="main-content" className="flex-1 relative min-w-0" aria-label="Graphic canvas" tabIndex={-1}>
          <Canvas />
        </main>
        {showPanels && (
          <div className="hidden md:flex">
            {sidebar === "layers" && (
              <div className="w-56 border-l border-zinc-800 bg-zinc-900">
                <LayerPanel />
              </div>
            )}
            {sidebar === "properties" && (
              <div className="w-64 border-l border-zinc-800 bg-zinc-900">
                <PropertiesPanel />
              </div>
            )}
          </div>
        )}
      </div>
      <div className="flex items-center justify-between h-8 px-3 bg-zinc-900 border-t border-zinc-800 text-[11px] text-zinc-600 shrink-0">
        <div className="flex gap-3">
          <button
            type="button"
            onClick={() => setSidebar(sidebar === "layers" ? null : "layers")}
            aria-pressed={sidebar === "layers"}
            className={`min-h-[44px] rounded px-2 hover:text-zinc-300 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${sidebar === "layers" ? "text-accent" : ""}`}
          >
            Layers
          </button>
          <button
            type="button"
            onClick={() => setSidebar(sidebar === "properties" ? null : "properties")}
            aria-pressed={sidebar === "properties"}
            className={`min-h-[44px] rounded px-2 hover:text-zinc-300 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${sidebar === "properties" ? "text-accent" : ""}`}
          >
            Properties
          </button>
          <button type="button" onClick={() => setShowPanels(!showPanels)} aria-pressed={showPanels} className="min-h-[44px] rounded px-2 hover:text-zinc-300 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent">
            {showPanels ? "Hide Panels" : "Show Panels"}
          </button>
        </div>
        <div className="flex gap-3">
          <span>FPS: 60</span>
          <span aria-live="polite">Project: {project?.name ?? "none"}</span>
        </div>
      </div>
    </div>
  );
}
