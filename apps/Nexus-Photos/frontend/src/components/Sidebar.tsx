import { useEditorStore } from "../stores/useEditorStore";

export default function Sidebar() {
  const albums = useEditorStore((s) => s.albums);
  const selectedAlbumId = useEditorStore((s) => s.selectedAlbumId);
  const setSelectedAlbumId = useEditorStore((s) => s.setSelectedAlbumId);
  const photos = useEditorStore((s) => s.photos);

  return (
    <nav aria-label="Photo albums" className="w-56 max-w-[45vw] border-r border-zinc-800 bg-zinc-900 flex flex-col shrink-0">
      <div className="px-3 py-2 text-xs font-medium text-zinc-500 uppercase tracking-wider">
        Albums
      </div>
      <div className="flex-1 overflow-y-auto">
        <button
          type="button"
          onClick={() => setSelectedAlbumId(null)}
          aria-current={selectedAlbumId === null ? "page" : undefined}
          className={`w-full min-h-[44px] text-left px-3 py-2 text-sm flex items-center justify-between hover:bg-zinc-800 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${
            selectedAlbumId === null ? "bg-zinc-800 text-accent" : "text-zinc-300"
          }`}
        >
          <span>All Photos</span>
          <span className="text-xs text-zinc-500">{photos.length}</span>
        </button>
        <button
          type="button"
          onClick={() => setSelectedAlbumId("favorites")}
          aria-current={selectedAlbumId === "favorites" ? "page" : undefined}
          className={`w-full min-h-[44px] text-left px-3 py-2 text-sm flex items-center justify-between hover:bg-zinc-800 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${
            selectedAlbumId === "favorites" ? "bg-zinc-800 text-accent" : "text-zinc-300"
          }`}
        >
          <span>Favorites</span>
          <span className="text-xs text-zinc-500">
            {photos.filter((p) => p.is_favorite).length}
          </span>
        </button>
        <div className="border-t border-zinc-800 my-1" />
        {albums.map((album) => (
          <button
            key={album.id}
            type="button"
            onClick={() => setSelectedAlbumId(album.id)}
            aria-current={selectedAlbumId === album.id ? "page" : undefined}
            className={`w-full min-h-[44px] text-left px-3 py-2 text-sm flex items-center justify-between hover:bg-zinc-800 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${
              selectedAlbumId === album.id ? "bg-zinc-800 text-accent" : "text-zinc-300"
            }`}
          >
            <span className="truncate">{album.name}</span>
            <span className="text-xs text-zinc-500">{album.photo_count}</span>
          </button>
        ))}
      </div>
    </nav>
  );
}
