import { useEffect, useCallback } from "react";
import TopBar from "./components/TopBar";
import Sidebar from "./components/Sidebar";
import PhotoGrid from "./components/PhotoGrid";
import PhotoViewer from "./components/PhotoViewer";
import PhotoEditor from "./components/PhotoEditor";
import { useEditorStore } from "./stores/useEditorStore";

export default function App() {
  const showSidebar = useEditorStore((s) => s.showSidebar);
  const viewMode = useEditorStore((s) => s.viewMode);
  const viewingPhoto = useEditorStore((s) => s.viewingPhoto);
  const setViewMode = useEditorStore((s) => s.setViewMode);
  const setShowSidebar = useEditorStore((s) => s.setShowSidebar);
  const deletePhotos = useEditorStore((s) => s.deletePhotos);
  const selectedPhotoIds = useEditorStore((s) => s.selectedPhotoIds);
  const photos = useEditorStore((s) => s.photos);
  const selectedAlbumId = useEditorStore((s) => s.selectedAlbumId);
  const triggerThumbnailGen = useEditorStore((s) => s.triggerThumbnailGen);

  useEffect(() => {
    const mode = viewMode === "grid" ? "Library" : viewMode === "detail" ? "Viewer" : "Editor";
    document.title = selectedAlbumId ? `${mode} — Photos` : `Nexus-Photos — ${mode}`;
  }, [selectedAlbumId, viewMode]);

  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      if (e.key === "g") setViewMode("grid");
      if (e.key === "d") setViewMode("detail");
      if (e.key === "e") setViewMode("edit");
      if (e.key === "/") {
        const el = document.querySelector<HTMLInputElement>("[data-search]");
        el?.focus();
      }
      if (e.key === "Delete" && selectedPhotoIds.size > 0) {
        if (confirm(`Delete ${selectedPhotoIds.size} selected photo(s)?`)) {
          deletePhotos([...selectedPhotoIds]);
        }
      }
    },
    [setViewMode, deletePhotos, selectedPhotoIds],
  );

  useEffect(() => {
    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [handleKeyDown]);

  const exif = viewingPhoto?.exif_data;

  return (
    <div className="h-screen flex flex-col bg-zinc-950 text-zinc-100 font-sans antialiased">
      <a
        href="#main-content"
        className="sr-only focus:not-sr-only focus:absolute focus:z-50 focus:left-2 focus:top-2 focus:rounded focus:bg-zinc-800 focus:px-4 focus:py-2 focus:text-zinc-100 focus:ring-2 focus:ring-accent focus:outline-none"
      >
        Skip to photo content
      </a>
      <TopBar />
      <div className="flex flex-1 overflow-hidden">
        {showSidebar && <Sidebar />}
        <main id="main-content" className="flex-1 min-w-0 overflow-auto" aria-label="Photo workspace" tabIndex={-1}>
          {viewMode === "grid" && <PhotoGrid />}
          {viewMode === "detail" && viewingPhoto && <PhotoViewer />}
          {viewMode === "edit" && viewingPhoto && <PhotoEditor />}
          {(viewMode === "detail" || viewMode === "edit") && !viewingPhoto && (
            <div className="flex flex-col items-center justify-center h-full text-zinc-500 gap-2">
              <span className="text-2xl">🔍</span>
              <span>Select a photo from the grid to inspect or edit</span>
              <button
                type="button"
                onClick={() => setViewMode("grid")}
                className="text-xs px-3 py-1.5 rounded bg-zinc-800 hover:bg-zinc-700 text-zinc-300 font-medium"
              >
                Return to Grid
              </button>
            </div>
          )}
        </main>

        {viewingPhoto && (viewMode === "detail" || viewMode === "edit") && (
          <aside aria-label="Photo metadata" className="hidden lg:block w-72 border-l border-zinc-800 bg-zinc-900/90 p-4 overflow-y-auto">
            <h3 className="text-xs font-semibold text-zinc-400 uppercase tracking-wider mb-3">
              Photo Info & Metadata
            </h3>
            <div className="text-xs space-y-3">
              <div>
                <span className="text-zinc-400 font-medium block">Title</span>
                <span className="text-zinc-200">{viewingPhoto.title || "Untitled"}</span>
              </div>
              <div className="grid grid-cols-2 gap-2">
                <div>
                  <span className="text-zinc-400 font-medium block">Dimensions</span>
                  <span className="text-zinc-200">{viewingPhoto.width} × {viewingPhoto.height}</span>
                </div>
                <div>
                  <span className="text-zinc-400 font-medium block">File Size</span>
                  <span className="text-zinc-200">{(viewingPhoto.file_size / 1024 / 1024).toFixed(1)} MB</span>
                </div>
              </div>

              {/* WebP Thumbnail Status */}
              <div className="p-2.5 rounded bg-zinc-950/60 border border-zinc-800/80 flex flex-col gap-1.5">
                <div className="flex items-center justify-between">
                  <span className="text-[11px] font-medium text-zinc-400">WebP Thumbnail</span>
                  <span className="text-[10px] px-1.5 py-0.5 rounded bg-emerald-950 text-emerald-400 border border-emerald-800/50">
                    Active
                  </span>
                </div>
                <button
                  type="button"
                  onClick={() => triggerThumbnailGen(viewingPhoto.id)}
                  className="text-[11px] px-2 py-1 rounded bg-zinc-800 hover:bg-zinc-700 text-zinc-300 text-left font-medium transition-colors"
                >
                  ⚡ Regenerate WebP Thumbnail
                </button>
              </div>

              {/* EXIF Section */}
              {exif && (
                <div className="border-t border-zinc-800 pt-3">
                  <div className="text-zinc-300 font-medium text-xs mb-2 flex items-center justify-between">
                    <span>EXIF Camera Data</span>
                    <span className="text-[10px] text-zinc-500 font-mono">v1.0 Parser</span>
                  </div>

                  <div className="space-y-2 bg-zinc-950/40 p-2.5 rounded border border-zinc-800/60 font-mono text-[11px]">
                    {exif.make && (
                      <div>
                        <span className="text-zinc-500 block text-[10px]">Camera Body</span>
                        <span className="text-zinc-200">{exif.make} {exif.model}</span>
                      </div>
                    )}
                    {exif.lens && (
                      <div>
                        <span className="text-zinc-500 block text-[10px]">Lens</span>
                        <span className="text-zinc-200">{exif.lens}</span>
                      </div>
                    )}
                    <div className="grid grid-cols-3 gap-1 pt-1 border-t border-zinc-800/40">
                      <div>
                        <span className="text-zinc-500 block text-[10px]">Shutter</span>
                        <span className="text-accent">{exif.exposureTime || "1/250s"}</span>
                      </div>
                      <div>
                        <span className="text-zinc-500 block text-[10px]">Aperture</span>
                        <span className="text-accent">{exif.fNumber ? `f/${exif.fNumber}` : "f/2.8"}</span>
                      </div>
                      <div>
                        <span className="text-zinc-500 block text-[10px]">ISO</span>
                        <span className="text-accent">{exif.iso || 200}</span>
                      </div>
                    </div>
                    {exif.gps && (
                      <div className="pt-1.5 border-t border-zinc-800/40">
                        <span className="text-zinc-500 block text-[10px]">GPS Coordinates</span>
                        <span className="text-zinc-300 text-[10px]">
                          {exif.gps.latitude.toFixed(4)}°N, {exif.gps.longitude.toFixed(4)}°E
                        </span>
                      </div>
                    )}
                  </div>
                </div>
              )}
            </div>
          </aside>
        )}
      </div>

      <div className="flex items-center justify-between h-8 px-3 bg-zinc-900 border-t border-zinc-800 text-[11px] text-zinc-500 shrink-0">
        <div className="flex gap-3">
          <span aria-live="polite">{photos.length} photos</span>
          <span>{selectedPhotoIds.size} selected</span>
        </div>
        <div className="flex gap-3">
          <button type="button" onClick={() => setShowSidebar(!showSidebar)} aria-pressed={showSidebar} className="hover:text-zinc-300">
            {showSidebar ? "Hide Sidebar" : "Show Sidebar"}
          </button>
          <span className="text-zinc-600">G=Grid D=Detail E=Edit Del=Delete</span>
        </div>
      </div>
    </div>
  );
}
