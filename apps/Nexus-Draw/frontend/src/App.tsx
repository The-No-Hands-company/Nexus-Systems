import { useState } from "react";
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

  return (
    <div className="h-screen flex flex-col">
      <TopBar />
      <div className="flex flex-1 overflow-hidden">
        <Toolbar />
        <div className="flex-1 relative">
          <Canvas />
        </div>
        {showPanels && (
          <div className="flex">
            {sidebar === "layers" && (
              <div className="w-56 border-l border-zinc-800 bg-zinc-900">
                <LayerPanel />
              </div>
            )}
            {sidebar === "properties" && (
              <div className="w-72 border-l border-zinc-800 bg-zinc-900">
                <PropertiesPanel />
              </div>
            )}
          </div>
        )}
      </div>
      <div className="flex items-center justify-between h-8 px-3 bg-zinc-900 border-t border-zinc-800 text-[11px] text-zinc-600 shrink-0">
        <div className="flex gap-3">
          <button
            onClick={() => setSidebar(sidebar === "layers" ? null : "layers")}
            className={`hover:text-zinc-300 ${sidebar === "layers" ? "text-blue-400" : ""}`}
          >
            Elements
          </button>
          <button
            onClick={() => setSidebar(sidebar === "properties" ? null : "properties")}
            className={`hover:text-zinc-300 ${sidebar === "properties" ? "text-blue-400" : ""}`}
          >
            Properties
          </button>
          <button onClick={() => setShowPanels(!showPanels)} className="hover:text-zinc-300">
            {showPanels ? "Hide" : "Show"}
          </button>
        </div>
        <div className="flex gap-3">
          <span>Board: {useEditorStore.getState().board?.name ?? "none"}</span>
        </div>
      </div>
    </div>
  );
}
