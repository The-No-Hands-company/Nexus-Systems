import { useState, useCallback } from "react";
import { useEditorStore } from "../stores/useEditorStore";

export default function TopBar() {
  const [search, setSearch] = useState("");
  const viewMode = useEditorStore((s) => s.viewMode);
  const setViewMode = useEditorStore((s) => s.setViewMode);
  const selectedAlbumId = useEditorStore((s) => s.selectedAlbumId);
  const showSidebar = useEditorStore((s) => s.showSidebar);
  const setShowSidebar = useEditorStore((s) => s.setShowSidebar);

  const handleSearch = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      setSearch(e.target.value);
    },
    [],
  );

  return (
    <header role="banner" className="flex flex-wrap items-center justify-between gap-2 min-h-12 px-4 py-1 bg-zinc-900 border-b border-zinc-800 shrink-0">
      <div className="flex items-center gap-3">
        <button type="button" aria-label="Toggle photo albums" aria-pressed={showSidebar} onClick={() => setShowSidebar(!showSidebar)} className="sm:hidden min-h-[44px] min-w-[44px] rounded text-zinc-400 hover:text-zinc-200 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent">☰</button>
        <span className="font-semibold text-sm text-zinc-200">Nexus-Photos</span>
        {selectedAlbumId && <span className="text-sm text-zinc-400">Album</span>}
      </div>
      <div className="flex items-center gap-3 flex-1 max-w-md mx-4">
        <div className="relative w-full">
          <input
            data-search
            aria-label="Search photos"
            type="text"
            value={search}
            onChange={handleSearch}
            placeholder="Search photos... (/)"
            className="w-full bg-zinc-800 text-zinc-200 text-xs rounded px-3 py-1.5 border border-zinc-700 focus:outline-none focus:border-accent focus-visible:ring-2 focus-visible:ring-accent placeholder-zinc-500"
          />
        </div>
      </div>
      <div className="flex items-center gap-2">
        <button
          type="button"
          onClick={() => setViewMode("grid")}
          aria-pressed={viewMode === "grid"}
          className={`min-h-[44px] text-xs px-3 py-1 rounded focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${viewMode === "grid" ? "bg-accent text-zinc-900" : "text-zinc-400 hover:text-zinc-200"}`}
        >
          Grid
        </button>
        <button
          type="button"
          onClick={() => setViewMode("detail")}
          aria-pressed={viewMode === "detail"}
          className={`min-h-[44px] text-xs px-3 py-1 rounded focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${viewMode === "detail" ? "bg-accent text-zinc-900" : "text-zinc-400 hover:text-zinc-200"}`}
        >
          Detail
        </button>
        <button
          type="button"
          onClick={() => setViewMode("edit")}
          aria-pressed={viewMode === "edit"}
          className={`min-h-[44px] text-xs px-3 py-1 rounded focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${viewMode === "edit" ? "bg-accent text-zinc-900" : "text-zinc-400 hover:text-zinc-200"}`}
        >
          Edit
        </button>
        <div className="w-px h-5 bg-zinc-700 mx-1" />
        <button type="button" aria-label="Upload photos" className="min-h-[44px] text-xs bg-accent hover:bg-accent-hover text-zinc-900 px-3 py-1.5 rounded focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent">
          Upload
        </button>
      </div>
    </header>
  );
}
