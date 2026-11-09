import { useState } from "react";
import { useEditorStore } from "../stores/useEditorStore";

export default function Sidebar() {
  const albums = useEditorStore((s) => s.albums);
  const selectedAlbumId = useEditorStore((s) => s.selectedAlbumId);
  const setSelectedAlbumId = useEditorStore((s) => s.setSelectedAlbumId);
  const photos = useEditorStore((s) => s.photos);
  const createAlbum = useEditorStore((s) => s.createAlbum);
  const deleteAlbum = useEditorStore((s) => s.deleteAlbum);

  const [isCreating, setIsCreating] = useState(false);
  const [newAlbumName, setNewAlbumName] = useState("");

  const handleCreateAlbum = (e: React.FormEvent) => {
    e.preventDefault();
    if (!newAlbumName.trim()) return;
    createAlbum(newAlbumName);
    setNewAlbumName("");
    setIsCreating(false);
  };

  return (
    <nav aria-label="Photo albums" className="w-64 max-w-[45vw] border-r border-zinc-800 bg-zinc-900 flex flex-col shrink-0">
      <div className="flex items-center justify-between px-3 py-2 border-b border-zinc-800">
        <span className="text-xs font-semibold text-zinc-400 uppercase tracking-wider">
          Library & Albums
        </span>
        <button
          type="button"
          onClick={() => setIsCreating(true)}
          className="text-xs px-2 py-0.5 rounded bg-zinc-800 hover:bg-zinc-700 text-zinc-300 font-medium transition-colors"
          title="Create New Album"
        >
          + New Album
        </button>
      </div>

      {isCreating && (
        <form onSubmit={handleCreateAlbum} className="p-2 border-b border-zinc-800 bg-zinc-950/60 flex flex-col gap-1.5">
          <input
            type="text"
            autoFocus
            value={newAlbumName}
            onChange={(e) => setNewAlbumName(e.target.value)}
            placeholder="Album title..."
            className="w-full text-xs bg-zinc-900 border border-zinc-700 rounded px-2 py-1 text-zinc-200 focus:outline-none focus:border-accent"
          />
          <div className="flex justify-end gap-1.5">
            <button
              type="button"
              onClick={() => { setIsCreating(false); setNewAlbumName(""); }}
              className="text-[11px] px-2 py-0.5 rounded text-zinc-400 hover:text-zinc-200"
            >
              Cancel
            </button>
            <button
              type="submit"
              className="text-[11px] px-2.5 py-0.5 rounded bg-accent text-zinc-900 font-medium hover:bg-accent-hover"
            >
              Save
            </button>
          </div>
        </form>
      )}

      <div className="flex-1 overflow-y-auto py-1">
        <button
          type="button"
          onClick={() => setSelectedAlbumId(null)}
          aria-current={selectedAlbumId === null ? "page" : undefined}
          className={`w-full min-h-[40px] text-left px-3 py-2 text-sm flex items-center justify-between hover:bg-zinc-800/80 transition-colors ${
            selectedAlbumId === null ? "bg-zinc-800 text-accent font-medium" : "text-zinc-300"
          }`}
        >
          <span className="flex items-center gap-2">
            <span>🖼️</span>
            <span>All Photos</span>
          </span>
          <span className="text-xs text-zinc-500">{photos.length}</span>
        </button>

        <button
          type="button"
          onClick={() => setSelectedAlbumId("favorites")}
          aria-current={selectedAlbumId === "favorites" ? "page" : undefined}
          className={`w-full min-h-[40px] text-left px-3 py-2 text-sm flex items-center justify-between hover:bg-zinc-800/80 transition-colors ${
            selectedAlbumId === "favorites" ? "bg-zinc-800 text-accent font-medium" : "text-zinc-300"
          }`}
        >
          <span className="flex items-center gap-2">
            <span>⭐</span>
            <span>Favorites</span>
          </span>
          <span className="text-xs text-zinc-500">
            {photos.filter((p) => p.is_favorite).length}
          </span>
        </button>

        <div className="border-t border-zinc-800/80 my-2 px-3 pt-2">
          <span className="text-[11px] font-semibold text-zinc-500 uppercase tracking-wider">
            Collections ({albums.length})
          </span>
        </div>

        {albums.map((album) => {
          const count = photos.filter((p) => p.album_id === album.id).length;
          return (
            <div
              key={album.id}
              className={`group flex items-center justify-between px-3 py-1.5 text-sm transition-colors ${
                selectedAlbumId === album.id ? "bg-zinc-800 text-accent font-medium" : "text-zinc-300 hover:bg-zinc-800/50"
              }`}
            >
              <button
                type="button"
                onClick={() => setSelectedAlbumId(album.id)}
                className="flex-1 text-left flex items-center gap-2 truncate py-1"
              >
                <span>📁</span>
                <span className="truncate">{album.name}</span>
              </button>
              <div className="flex items-center gap-2">
                <span className="text-xs text-zinc-500">{count}</span>
                <button
                  type="button"
                  onClick={(e) => {
                    e.stopPropagation();
                    if (confirm(`Delete album "${album.name}"? Photos will not be deleted.`)) {
                      deleteAlbum(album.id);
                    }
                  }}
                  className="opacity-0 group-hover:opacity-100 text-zinc-500 hover:text-red-400 text-xs px-1"
                  title="Delete album"
                >
                  ✕
                </button>
              </div>
            </div>
          );
        })}
      </div>
    </nav>
  );
}
