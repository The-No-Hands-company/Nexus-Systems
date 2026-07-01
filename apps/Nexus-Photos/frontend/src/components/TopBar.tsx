import { useState, useCallback } from "react";
import { useEditorStore } from "../stores/useEditorStore";

export default function TopBar() {
  const [search, setSearch] = useState("");
  const viewMode = useEditorStore((s) => s.viewMode);
  const setViewMode = useEditorStore((s) => s.setViewMode);
  const selectedAlbumId = useEditorStore((s) => s.selectedAlbumId);

  const handleSearch = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      setSearch(e.target.value);
    },
    [],
  );

  return (
    <div className="flex items-center justify-between h-12 px-4 bg-zinc-900 border-b border-zinc-800 shrink-0">
      <div className="flex items-center gap-3">
        <span className="font-semibold text-sm text-zinc-200">Nexus-Photos</span>
        {selectedAlbumId && <span className="text-sm text-zinc-400">Album</span>}
      </div>
      <div className="flex items-center gap-3 flex-1 max-w-md mx-4">
        <div className="relative w-full">
          <input
            data-search
            type="text"
            value={search}
            onChange={handleSearch}
            placeholder="Search photos... (/)"
            className="w-full bg-zinc-800 text-zinc-200 text-xs rounded px-3 py-1.5 border border-zinc-700 focus:outline-none focus:border-blue-500 placeholder-zinc-500"
          />
        </div>
      </div>
      <div className="flex items-center gap-2">
        <button
          onClick={() => setViewMode("grid")}
          className={`text-xs px-2 py-1 rounded ${viewMode === "grid" ? "bg-blue-600 text-white" : "text-zinc-400 hover:text-zinc-200"}`}
        >
          Grid
        </button>
        <button
          onClick={() => setViewMode("detail")}
          className={`text-xs px-2 py-1 rounded ${viewMode === "detail" ? "bg-blue-600 text-white" : "text-zinc-400 hover:text-zinc-200"}`}
        >
          Detail
        </button>
        <button
          onClick={() => setViewMode("edit")}
          className={`text-xs px-2 py-1 rounded ${viewMode === "edit" ? "bg-blue-600 text-white" : "text-zinc-400 hover:text-zinc-200"}`}
        >
          Edit
        </button>
        <div className="w-px h-5 bg-zinc-700 mx-1" />
        <button className="text-xs bg-blue-600 hover:bg-blue-500 text-white px-3 py-1.5 rounded">
          Upload
        </button>
      </div>
    </div>
  );
}
