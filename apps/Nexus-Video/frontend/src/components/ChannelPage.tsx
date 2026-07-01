import { useEditorStore } from "../stores/useEditorStore";

export default function ChannelPage() {
  const channels = useEditorStore((s) => s.channels);
  const selectedChannelId = useEditorStore((s) => s.selectedChannelId);
  const videos = useEditorStore((s) => s.videos);
  const setCurrentlyPlaying = useEditorStore((s) => s.setCurrentlyPlaying);

  const channel = channels.find((c) => c.id === selectedChannelId);
  if (!channel) return <div className="p-8 text-zinc-500">Channel not found</div>;

  const channelVideos = videos.filter((v) => v.channel_id === channel.id);

  return (
    <div>
      <div
        className="h-48 bg-zinc-800 bg-cover bg-center"
        style={channel.banner_url ? { backgroundImage: `url(${channel.banner_url})` } : undefined}
      >
        <div className="h-full bg-gradient-to-t from-black/60 to-transparent flex items-end p-6">
          <div className="flex items-center gap-4">
            <div className="w-20 h-20 rounded-full bg-zinc-700 flex items-center justify-center text-2xl font-bold text-zinc-400 shrink-0 border-2 border-white/20">
              {channel.name.charAt(0).toUpperCase()}
            </div>
            <div>
              <div className="flex items-center gap-2">
                <h1 className="text-2xl font-bold text-white">{channel.name}</h1>
                {channel.is_verified && <span className="text-blue-400 text-lg" title="Verified">✓</span>}
              </div>
              <p className="text-sm text-zinc-400">
                {channel.subscriber_count.toLocaleString()} subscribers · {channel.video_count} videos
              </p>
              {channel.description && (
                <p className="text-sm text-zinc-500 mt-1 max-w-xl line-clamp-2">{channel.description}</p>
              )}
            </div>
          </div>
        </div>
      </div>
      <div className="px-6 py-4 border-b border-zinc-800 flex items-center gap-4">
        <button className="px-4 py-1.5 bg-red-600 hover:bg-red-700 text-white text-sm font-medium rounded-full">
          Subscribe
        </button>
      </div>
      <div className="p-4">
        {channelVideos.length === 0 ? (
          <p className="text-zinc-500 text-center py-12">No videos uploaded yet</p>
        ) : (
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
            {channelVideos.map((v) => (
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
                      <div className="w-10 h-10 rounded-full bg-zinc-700 flex items-center justify-center">
                        <div className="w-0 h-0 border-t-6 border-b-6 border-l-10 border-transparent border-l-white ml-0.5" />
                      </div>
                    </div>
                  )}
                </div>
                <h3 className="text-sm font-medium text-zinc-100 line-clamp-2">{v.title}</h3>
                <p className="text-xs text-zinc-500 mt-1">{v.views.toLocaleString()} views</p>
              </button>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
