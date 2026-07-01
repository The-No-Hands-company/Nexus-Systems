import { useEditorStore } from "../stores/useEditorStore";
import { Search, Upload, Grid3X3, List, Menu } from "lucide-react";

export default function TopBar() {
  const viewMode = useEditorStore((s) => s.viewMode);
  const setViewMode = useEditorStore((s) => s.setViewMode);
  const showSidebar = useEditorStore((s) => s.showSidebar);
  const setShowSidebar = useEditorStore((s) => s.setShowSidebar);

  return (
    <div className="flex items-center justify-between h-12 px-4 bg-zinc-900 border-b border-zinc-800 shrink-0">
      <div className="flex items-center gap-4">
        <button
          onClick={() => setShowSidebar(!showSidebar)}
          className="text-zinc-400 hover:text-zinc-200"
        >
          <Menu size={20} />
        </button>
        <span className="font-bold text-lg text-white">Nexus-Video</span>
      </div>
      <div className="flex-1 max-w-xl mx-4">
        <div className="relative">
          <Search size={16} className="absolute left-3 top-1/2 -translate-y-1/2 text-zinc-500" />
          <input
            type="text"
            placeholder="Search videos..."
            className="w-full bg-zinc-800 border border-zinc-700 rounded-full py-1.5 pl-9 pr-4 text-sm text-zinc-200 placeholder-zinc-500 focus:outline-none focus:border-blue-500"
          />
        </div>
      </div>
      <div className="flex items-center gap-2">
        <button
          onClick={() => setViewMode(viewMode === "grid" ? "list" : "grid")}
          className="p-2 text-zinc-400 hover:text-zinc-200 rounded-md hover:bg-zinc-800"
        >
          {viewMode === "grid" ? <List size={18} /> : <Grid3X3 size={18} />}
        </button>
        <button className="flex items-center gap-1.5 px-3 py-1.5 bg-blue-600 hover:bg-blue-700 text-white text-sm font-medium rounded-full">
          <Upload size={16} />
          Upload
        </button>
      </div>
    </div>
  );
}
