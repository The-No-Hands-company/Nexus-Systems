import { useEditorStore } from "../stores/useEditorStore";

function formatViews(n: number): string {
  if (n >= 1_000_000) return `${(n / 1_000_000).toFixed(1)}M`;
  if (n >= 1_000) return `${(n / 1_000).toFixed(1)}K`;
  return n.toString();
}

function formatDuration(s: number): string {
  const m = Math.floor(s / 60);
  const sec = Math.floor(s % 60);
  return `${m}:${sec.toString().padStart(2, "0")}`;
}

function timeAgo(dateStr: string): string {
  const diff = Date.now() - new Date(dateStr).getTime();
  const days = Math.floor(diff / 86400000);
  if (days > 365) return `${Math.floor(days / 365)}y ago`;
  if (days > 30) return `${Math.floor(days / 30)}mo ago`;
  if (days > 0) return `${days}d ago`;
  return "today";
}

export default function VideoGrid() {
  const videos = useEditorStore((s) => s.videos);
  const setCurrentlyPlaying = useEditorStore((s) => s.setCurrentlyPlaying);

  return (
    <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
      {videos.length === 0 && (
        <div className="col-span-full text-center py-20 text-zinc-500">
          <p className="text-lg">No videos yet</p>
          <p className="text-sm mt-1">Upload a video to get started</p>
        </div>
      )}
      {videos.map((v) => (
        <button
          key={v.id}
          onClick={() => setCurrentlyPlaying(v)}
          className="text-left group"
        >
          <div className="relative aspect-video bg-zinc-800 rounded-xl overflow-hidden mb-2">
            {v.thumbnail_url ? (
              <img src={v.thumbnail_url} alt={v.title} className="w-full h-full object-cover" />
            ) : (
              <div className="w-full h-full flex items-center justify-center text-zinc-600">
                <div className="w-12 h-12 rounded-full bg-zinc-700 flex items-center justify-center">
                  <div className="w-0 h-0 border-t-8 border-b-8 border-l-12 border-transparent border-l-white ml-1" />
                </div>
              </div>
            )}
            {v.duration > 0 && (
              <span className="absolute bottom-1 right-1 bg-black/80 text-white text-[11px] px-1.5 py-0.5 rounded font-medium">
                {formatDuration(v.duration)}
              </span>
            )}
          </div>
          <div className="flex gap-2">
            <div className="w-9 h-9 rounded-full bg-zinc-700 shrink-0 flex items-center justify-center text-xs font-bold text-zinc-500">
              {(v.channel_name || "?").charAt(0).toUpperCase()}
            </div>
            <div className="min-w-0">
              <h3 className="text-sm font-medium text-zinc-100 line-clamp-2 leading-tight">{v.title}</h3>
              <p className="text-xs text-zinc-400 mt-1">{v.channel_name || "Unknown"}</p>
              <p className="text-xs text-zinc-500">
                {formatViews(v.views)} views · {timeAgo(v.created_at)}
              </p>
            </div>
          </div>
        </button>
      ))}
    </div>
  );
}
