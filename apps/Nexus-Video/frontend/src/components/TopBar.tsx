import { useEditorStore } from "../stores/useEditorStore";
import Search from "lucide-react/dist/esm/icons/search.js";
import Upload from "lucide-react/dist/esm/icons/upload.js";
import Grid3X3 from "lucide-react/dist/esm/icons/grid-3x3.js";
import List from "lucide-react/dist/esm/icons/list.js";
import Menu from "lucide-react/dist/esm/icons/menu.js";

export default function TopBar() {
  const viewMode = useEditorStore((s) => s.viewMode);
  const setViewMode = useEditorStore((s) => s.setViewMode);
  const showSidebar = useEditorStore((s) => s.showSidebar);
  const setShowSidebar = useEditorStore((s) => s.setShowSidebar);

  return (
    <header role="banner" className="flex items-center justify-between gap-2 min-h-12 px-4 py-1 bg-zinc-900 border-b border-zinc-800 shrink-0">
      <div className="flex items-center gap-4">
        <button
          onClick={() => setShowSidebar(!showSidebar)}
          type="button"
          aria-label="Toggle video navigation"
          aria-pressed={showSidebar}
          className="min-h-[44px] min-w-[44px] rounded text-zinc-400 hover:text-zinc-200 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
        >
          <Menu size={20} aria-hidden="true" />
        </button>
        <span className="font-bold text-lg text-white">Nexus-Video</span>
      </div>
      <div className="flex-1 max-w-xl mx-4">
        <div className="relative">
          <Search size={16} className="absolute left-3 top-1/2 -translate-y-1/2 text-zinc-500" />
          <input
            type="text"
            aria-label="Search videos"
            placeholder="Search videos..."
            className="w-full bg-zinc-800 border border-zinc-700 rounded-full py-1.5 pl-9 pr-4 text-sm text-zinc-200 placeholder-zinc-500 focus:outline-none focus:border-accent focus-visible:ring-2 focus-visible:ring-accent"
          />
        </div>
      </div>
      <div className="flex items-center gap-2">
        <button
          type="button"
          onClick={() => setViewMode(viewMode === "grid" ? "list" : "grid")}
          aria-label={viewMode === "grid" ? "Switch to list view" : "Switch to grid view"}
          aria-pressed={viewMode === "list"}
          className="min-h-[44px] min-w-[44px] p-2 text-zinc-400 hover:text-zinc-200 rounded-md hover:bg-zinc-800 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
        >
          {viewMode === "grid" ? <List size={18} /> : <Grid3X3 size={18} />}
        </button>
        <button type="button" aria-label="Upload video" className="flex items-center gap-1.5 min-h-[44px] px-3 py-1.5 bg-accent hover:bg-accent-hover text-zinc-900 text-sm font-medium rounded-full focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent">
          <Upload size={16} aria-hidden="true" />
          Upload
        </button>
      </div>
    </header>
  );
}
