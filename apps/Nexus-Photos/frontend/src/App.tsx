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
        if (confirm("Delete selected photos?")) {
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

  return (
    <div className="h-screen flex flex-col">
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
            <div className="flex items-center justify-center h-full text-zinc-600">
              Select a photo to view
            </div>
          )}
        </main>
        {viewingPhoto && (viewMode === "detail" || viewMode === "edit") && (
          <aside aria-label="Photo details" className="hidden lg:block w-64 border-l border-zinc-800 bg-zinc-900 p-4 overflow-y-auto">
            <div className="text-xs text-zinc-500 space-y-2">
              <div><span className="text-zinc-400">File:</span> {viewingPhoto.title || "untitled"}</div>
              <div><span className="text-zinc-400">Size:</span> {viewingPhoto.width}×{viewingPhoto.height}</div>
              <div><span className="text-zinc-400">Type:</span> {viewingPhoto.content_type}</div>
              <div><span className="text-zinc-400">Created:</span> {new Date(viewingPhoto.created_at).toLocaleDateString()}</div>
              {Object.entries(viewingPhoto.exif_data).length > 0 && (
                <div className="border-t border-zinc-800 pt-2 mt-2">
                  <div className="text-zinc-400 mb-1 font-medium">EXIF</div>
                  {Object.entries(viewingPhoto.exif_data).map(([k, v]) => (
                    <div key={k}><span className="text-zinc-500">{k}:</span> {String(v)}</div>
                  ))}
                </div>
              )}
            </div>
          </aside>
        )}
      </div>
      <div className="flex items-center justify-between h-8 px-3 bg-zinc-900 border-t border-zinc-800 text-[11px] text-zinc-600 shrink-0">
        <div className="flex gap-3">
          <span aria-live="polite">{photos.length} photos</span>
          <span>{selectedPhotoIds.size} selected</span>
        </div>
        <div className="flex gap-3">
          <button type="button" onClick={() => setShowSidebar(!showSidebar)} aria-pressed={showSidebar} className="min-h-[44px] rounded px-2 hover:text-zinc-300 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent">
            {showSidebar ? "Hide Sidebar" : "Show Sidebar"}
          </button>
          <span className="text-zinc-700">G=Grid D=Detail E=Edit</span>
        </div>
      </div>
    </div>
  );
}
