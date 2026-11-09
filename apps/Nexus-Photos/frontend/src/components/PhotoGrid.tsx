import { useState } from "react";
import { useEditorStore, type PhotoData } from "../stores/useEditorStore";

function PhotoThumb({ photo }: { photo: PhotoData }) {
  const selectPhoto = useEditorStore((s) => s.selectPhoto);
  const setViewingPhoto = useEditorStore((s) => s.setViewingPhoto);
  const setViewMode = useEditorStore((s) => s.setViewMode);
  const toggleFavorite = useEditorStore((s) => s.toggleFavorite);
  const selectedPhotoIds = useEditorStore((s) => s.selectedPhotoIds);
  const isSelected = selectedPhotoIds.has(photo.id);

  const handleClick = (e: React.MouseEvent) => {
    selectPhoto(photo.id, e.ctrlKey || e.metaKey || e.shiftKey);
  };

  const handleDoubleClick = () => {
    setViewingPhoto(photo);
    setViewMode("detail");
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === "Enter") handleDoubleClick();
    if (e.key === " ") {
      e.preventDefault();
      selectPhoto(photo.id, e.ctrlKey || e.metaKey || e.shiftKey);
    }
  };

  return (
    <button
      type="button"
      onClick={handleClick}
      onDoubleClick={handleDoubleClick}
      onKeyDown={handleKeyDown}
      aria-label={`${photo.title || "Untitled photo"}${isSelected ? ", selected" : ""}`}
      aria-pressed={isSelected}
      className={`group relative aspect-[4/3] rounded-lg overflow-hidden cursor-pointer border-2 transition-all bg-zinc-950 text-left focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${
        isSelected ? "border-accent ring-2 ring-accent/40 shadow-lg shadow-accent/10" : "border-zinc-800 hover:border-zinc-600"
      }`}
    >
      <img
        src={photo.thumbnail_url || photo.url}
        alt={photo.title}
        className="w-full h-full object-cover transition-transform duration-300 group-hover:scale-105"
        loading="lazy"
      />

      <div className="absolute inset-0 bg-gradient-to-t from-black/80 via-black/10 to-black/30 opacity-0 group-hover:opacity-100 transition-opacity pointer-events-none" />

      {/* Select Checkbox */}
      <div
        onClick={(e) => {
          e.stopPropagation();
          selectPhoto(photo.id, true);
        }}
        className={`absolute top-2 left-2 z-10 w-5 h-5 rounded border flex items-center justify-center transition-all ${
          isSelected
            ? "bg-accent border-accent text-zinc-900"
            : "border-white/50 bg-black/40 text-transparent opacity-0 group-hover:opacity-100"
        }`}
      >
        ✓
      </div>

      {/* Top right Favorite & WebP badge */}
      <div className="absolute top-2 right-2 flex items-center gap-1.5 z-10">
        <span className="text-[10px] uppercase font-mono px-1.5 py-0.5 rounded bg-zinc-900/80 text-zinc-300 backdrop-blur-sm border border-zinc-700/50">
          WebP
        </span>
        <button
          type="button"
          onClick={(e) => {
            e.stopPropagation();
            toggleFavorite(photo.id);
          }}
          className={`p-1 rounded bg-zinc-900/80 backdrop-blur-sm transition-colors ${
            photo.is_favorite ? "text-yellow-400" : "text-zinc-400 hover:text-yellow-400"
          }`}
          title={photo.is_favorite ? "Remove from Favorites" : "Add to Favorites"}
        >
          {photo.is_favorite ? "★" : "☆"}
        </button>
      </div>

      {/* Bottom info */}
      <div className="absolute bottom-0 left-0 right-0 p-2.5 opacity-0 group-hover:opacity-100 transition-opacity">
        <div className="text-xs font-medium text-white truncate drop-shadow-sm">{photo.title || "Untitled"}</div>
        <div className="flex items-center justify-between text-[10px] text-zinc-300 mt-0.5 font-mono">
          <span>{photo.width}×{photo.height}</span>
          <span>{photo.exif_data?.model || "Standard"}</span>
        </div>
      </div>
    </button>
  );
}

export default function PhotoGrid() {
  const photos = useEditorStore((s) => s.photos);
  const albums = useEditorStore((s) => s.albums);
  const selectedAlbumId = useEditorStore((s) => s.selectedAlbumId);
  const selectedPhotoIds = useEditorStore((s) => s.selectedPhotoIds);
  const selectAll = useEditorStore((s) => s.selectAll);
  const clearSelection = useEditorStore((s) => s.clearSelection);
  const deletePhotos = useEditorStore((s) => s.deletePhotos);
  const batchMoveToAlbum = useEditorStore((s) => s.batchMoveToAlbum);
  const batchDownload = useEditorStore((s) => s.batchDownload);

  const [targetAlbum, setTargetAlbum] = useState("");

  const filtered = photos.filter((p) => {
    if (selectedAlbumId === "favorites") return p.is_favorite;
    if (selectedAlbumId) return p.album_id === selectedAlbumId;
    return true;
  });

  const selectedCount = selectedPhotoIds.size;

  return (
    <div className="p-4 flex flex-col gap-4">
      {/* Batch Actions Floating / Sticky Bar */}
      {selectedCount > 0 && (
        <div className="sticky top-2 z-20 flex flex-wrap items-center justify-between gap-3 px-4 py-2.5 bg-zinc-900/95 border border-accent/40 rounded-xl shadow-2xl backdrop-blur-md">
          <div className="flex items-center gap-3">
            <span className="text-xs font-semibold px-2.5 py-1 rounded-full bg-accent/20 text-accent border border-accent/30">
              {selectedCount} Selected
            </span>
            <button
              type="button"
              onClick={selectAll}
              className="text-xs text-zinc-300 hover:text-white underline underline-offset-2"
            >
              Select All ({filtered.length})
            </button>
            <button
              type="button"
              onClick={clearSelection}
              className="text-xs text-zinc-400 hover:text-zinc-200"
            >
              Deselect
            </button>
          </div>

          <div className="flex items-center gap-2">
            {/* Move to Album Dropdown */}
            <div className="flex items-center gap-1.5">
              <select
                value={targetAlbum}
                onChange={(e) => setTargetAlbum(e.target.value)}
                className="text-xs bg-zinc-800 border border-zinc-700 rounded px-2.5 py-1 text-zinc-200 focus:outline-none focus:border-accent"
              >
                <option value="">Move to Album...</option>
                {albums.map((a) => (
                  <option key={a.id} value={a.id}>
                    {a.name}
                  </option>
                ))}
              </select>
              <button
                type="button"
                disabled={!targetAlbum}
                onClick={() => {
                  if (targetAlbum) {
                    batchMoveToAlbum(targetAlbum);
                    setTargetAlbum("");
                  }
                }}
                className="text-xs px-2.5 py-1 rounded bg-zinc-800 hover:bg-zinc-700 text-zinc-200 disabled:opacity-50 font-medium"
              >
                Move
              </button>
            </div>

            <button
              type="button"
              onClick={batchDownload}
              className="text-xs px-3 py-1 rounded bg-zinc-800 hover:bg-zinc-700 text-zinc-200 font-medium flex items-center gap-1"
            >
              <span>⬇</span> Download (ZIP)
            </button>

            <button
              type="button"
              onClick={() => {
                if (confirm(`Delete ${selectedCount} selected photo(s)?`)) {
                  deletePhotos(Array.from(selectedPhotoIds));
                }
              }}
              className="text-xs px-3 py-1 rounded bg-red-600/80 hover:bg-red-600 text-white font-medium flex items-center gap-1"
            >
              <span>🗑</span> Delete
            </button>
          </div>
        </div>
      )}

      {/* Photos Grid */}
      <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-3.5">
        {filtered.map((photo) => (
          <PhotoThumb key={photo.id} photo={photo} />
        ))}
      </div>

      {filtered.length === 0 && (
        <div className="flex flex-col items-center justify-center h-64 text-zinc-500 text-sm gap-2">
          <span className="text-3xl">📷</span>
          <span>No photos found in this view</span>
          <span className="text-xs text-zinc-600">Upload or import photos to get started</span>
        </div>
      )}
    </div>
  );
}
