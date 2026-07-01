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

  return (
    <div className="h-screen flex flex-col">
      <TopBar />
      <div className="flex flex-1 overflow-hidden">
        {showSidebar && (
          <div className="w-60 border-r border-zinc-800 bg-zinc-900 shrink-0">
            <Sidebar />
          </div>
        )}
        <div className="flex-1 flex flex-col overflow-hidden">
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
        </div>
        <div className="w-72 border-l border-zinc-800 bg-zinc-900 shrink-0 overflow-y-auto p-3">
          <h3 className="text-sm font-semibold text-zinc-400 uppercase tracking-wider mb-3">Playlist</h3>
          <div className="space-y-1 text-sm text-zinc-500">
            <p>Select a playlist to view</p>
          </div>
          <div className="mt-6 border-t border-zinc-800 pt-4">
            <h3 className="text-sm font-semibold text-zinc-400 uppercase tracking-wider mb-3">Visualization</h3>
            <Canvas />
          </div>
        </div>
      </div>
    </div>
  );
}
