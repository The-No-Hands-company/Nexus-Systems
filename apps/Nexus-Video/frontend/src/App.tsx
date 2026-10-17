import { useEffect } from "react";
import TopBar from "./components/TopBar";
import Sidebar from "./components/Sidebar";
import VideoGrid from "./components/VideoGrid";
import VideoPlayer from "./components/VideoPlayer";
import ChannelPage from "./components/ChannelPage";
import Canvas from "./components/Canvas/Canvas";
import { useEditorStore } from "./stores/useEditorStore";

export default function App() {
  const viewMode = useEditorStore((s) => s.viewMode);
  const showSidebar = useEditorStore((s) => s.showSidebar);
  const currentlyPlaying = useEditorStore((s) => s.currentlyPlaying);
  const selectedChannelId = useEditorStore((s) => s.selectedChannelId);
  const selectedPlaylistId = useEditorStore((s) => s.selectedPlaylistId);

  useEffect(() => {
    document.title = currentlyPlaying
      ? `${currentlyPlaying.title} — Video`
      : selectedChannelId
        ? "Channel — Nexus-Video"
        : selectedPlaylistId
          ? "Playlist — Nexus-Video"
          : "Nexus-Video";
  }, [currentlyPlaying, selectedChannelId, selectedPlaylistId]);

  return (
    <div className="h-screen flex flex-col">
      <a
        href="#main-content"
        className="sr-only focus:not-sr-only focus:absolute focus:z-50 focus:left-2 focus:top-2 focus:rounded focus:bg-zinc-800 focus:px-4 focus:py-2 focus:text-zinc-100 focus:ring-2 focus:ring-accent focus:outline-none"
      >
        Skip to video content
      </a>
      <TopBar />
      <div className="flex flex-1 overflow-hidden">
        {showSidebar && (
          <aside aria-label="Video navigation" className="hidden sm:block w-60 border-r border-zinc-800 bg-zinc-900 shrink-0">
            <Sidebar />
          </aside>
        )}
        <main id="main-content" className="flex-1 min-w-0 flex flex-col overflow-hidden" aria-label="Video workspace" tabIndex={-1}>
          {currentlyPlaying ? (
            <div className="flex-1 flex flex-col">
              <VideoPlayer />
              <div className="flex-1 overflow-y-auto p-4">
                {viewMode === "grid" ? <VideoGrid /> : <VideoGrid />}
              </div>
            </div>
          ) : selectedChannelId ? (
            <div className="flex-1 overflow-y-auto">
              <ChannelPage />
            </div>
          ) : (
            <div className="flex-1 overflow-y-auto p-4">
              <VideoGrid />
            </div>
          )}
        </main>
        <aside aria-label="Playlist and visualization" className="hidden xl:block w-72 border-l border-zinc-800 bg-zinc-900 shrink-0 overflow-y-auto p-3">
          <h3 className="text-sm font-semibold text-zinc-400 uppercase tracking-wider mb-3">Playlist</h3>
          <div className="space-y-1 text-sm text-zinc-500">
            <p>Select a playlist to view</p>
          </div>
          <div className="mt-6 border-t border-zinc-800 pt-4">
            <h3 className="text-sm font-semibold text-zinc-400 uppercase tracking-wider mb-3">Visualization</h3>
            <Canvas />
          </div>
        </aside>
      </div>
    </div>
  );
}
