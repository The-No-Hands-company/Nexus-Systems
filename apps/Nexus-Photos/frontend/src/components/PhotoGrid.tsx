import { useEditorStore, type PhotoData } from "../stores/useEditorStore";

function PhotoThumb({ photo }: { photo: PhotoData }) {
  const selectPhoto = useEditorStore((s) => s.selectPhoto);
  const setViewingPhoto = useEditorStore((s) => s.setViewingPhoto);
  const setViewMode = useEditorStore((s) => s.setViewMode);
  const selectedPhotoIds = useEditorStore((s) => s.selectedPhotoIds);
  const isSelected = selectedPhotoIds.has(photo.id);

  const handleClick = (e: React.MouseEvent) => {
    selectPhoto(photo.id, e.ctrlKey || e.metaKey);
  };

  const handleDoubleClick = () => {
    setViewingPhoto(photo);
    setViewMode("detail");
  };

  return (
    <div
      onClick={handleClick}
      onDoubleClick={handleDoubleClick}
      className={`relative aspect-square rounded-lg overflow-hidden cursor-pointer border-2 transition-all group ${
        isSelected ? "border-blue-500 ring-2 ring-blue-500/30" : "border-transparent hover:border-zinc-600"
      }`}
    >
      <img
        src={photo.thumbnail_url || photo.url}
        alt={photo.title}
        className="w-full h-full object-cover"
        loading="lazy"
      />
      <div className="absolute inset-0 bg-black/0 group-hover:bg-black/10 transition-colors" />
      {photo.is_favorite && (
        <div className="absolute top-1.5 right-1.5 text-yellow-400 text-xs">★</div>
      )}
      <div className="absolute bottom-0 left-0 right-0 bg-gradient-to-t from-black/60 to-transparent p-2 opacity-0 group-hover:opacity-100 transition-opacity">
        <div className="text-[11px] text-white truncate">{photo.title || "untitled"}</div>
        <div className="text-[10px] text-zinc-400">{photo.width}×{photo.height}</div>
      </div>
    </div>
  );
}

export default function PhotoGrid() {
  const photos = useEditorStore((s) => s.photos);
  const selectedAlbumId = useEditorStore((s) => s.selectedAlbumId);

  const filtered = photos.filter((p) => {
    if (selectedAlbumId === "favorites") return p.is_favorite;
    if (selectedAlbumId) return p.album_id === selectedAlbumId;
    return true;
  });

  return (
    <div className="p-4">
      <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 xl:grid-cols-6 gap-3">
        {filtered.map((photo) => (
          <PhotoThumb key={photo.id} photo={photo} />
        ))}
      </div>
      {filtered.length === 0 && (
        <div className="flex items-center justify-center h-64 text-zinc-600 text-sm">
          No photos found
        </div>
      )}
    </div>
  );
}
