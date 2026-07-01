import { useEditorStore } from "../stores/useEditorStore";

export default function Sidebar() {
  const channels = useEditorStore((s) => s.channels);
  const selectedChannelId = useEditorStore((s) => s.selectedChannelId);
  const setSelectedChannelId = useEditorStore((s) => s.setSelectedChannelId);
  const setCurrentlyPlaying = useEditorStore((s) => s.setCurrentlyPlaying);

  return (
    <div className="h-full flex flex-col py-2">
      <div className="px-3 mb-2">
        <h3 className="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Subscriptions</h3>
      </div>
      <div className="flex-1 overflow-y-auto space-y-0.5 px-2">
        {channels.length === 0 && (
          <p className="text-xs text-zinc-600 px-2 py-1">No channels yet</p>
        )}
        {channels.map((ch) => (
          <button
            key={ch.id}
            onClick={() => {
              setSelectedChannelId(ch.id);
              setCurrentlyPlaying(null);
            }}
            className={`flex items-center gap-3 w-full px-2 py-1.5 rounded-md text-sm transition-colors ${
              selectedChannelId === ch.id
                ? "bg-zinc-700 text-white"
                : "text-zinc-300 hover:bg-zinc-800"
            }`}
          >
            <div className="w-8 h-8 rounded-full bg-zinc-700 shrink-0 flex items-center justify-center text-xs font-bold text-zinc-400">
              {ch.name.charAt(0).toUpperCase()}
            </div>
            <div className="flex-1 text-left min-w-0">
              <div className="truncate">{ch.name}</div>
              <div className="text-[11px] text-zinc-500">
                {ch.subscriber_count.toLocaleString()} subscribers
              </div>
            </div>
            {ch.is_verified && (
              <span className="text-blue-400 text-xs shrink-0" title="Verified">✓</span>
            )}
          </button>
        ))}
      </div>
      <div className="border-t border-zinc-800 pt-2 mt-2 px-3">
        <h3 className="text-xs font-semibold text-zinc-400 uppercase tracking-wider mb-1">Library</h3>
        <button
          onClick={() => { setSelectedChannelId(null); setCurrentlyPlaying(null); }}
          className="block w-full text-left text-sm text-zinc-400 hover:text-zinc-200 py-1 px-2 rounded hover:bg-zinc-800"
        >
          All Videos
        </button>
        <button className="block w-full text-left text-sm text-zinc-400 hover:text-zinc-200 py-1 px-2 rounded hover:bg-zinc-800">
          History
        </button>
        <button className="block w-full text-left text-sm text-zinc-400 hover:text-zinc-200 py-1 px-2 rounded hover:bg-zinc-800">
          Liked Videos
        </button>
      </div>
    </div>
  );
}
