import { create } from "zustand";

export interface ChapterCue {
  time: number;
  title: string;
}

export interface SubtitleTrack {
  srclang: string;
  label: string;
  url: string;
}

export interface Channel {
  id: string;
  name: string;
  description: string;
  avatar_url?: string;
  banner_url?: string;
  subscriber_count: number;
  video_count: number;
  is_verified?: boolean;
}

export interface Video {
  id: string;
  channel_id: string;
  title: string;
  description: string;
  url: string;
  hls_url?: string;
  thumbnail_url: string;
  duration: number;
  width?: number;
  height?: number;
  file_size?: number;
  content_type?: string;
  tags?: string;
  views: number;
  likes?: number;
  is_published?: boolean;
  status?: string;
  channel_name?: string;
  chapters?: ChapterCue[];
  subtitles?: SubtitleTrack[];
  saved_progress?: number;
  created_at: string;
}

export interface Playlist {
  id: string;
  name: string;
  description: string;
  channel_id?: string;
  video_ids: string[];
  is_public: boolean;
}

interface EditorStore {
  channels: Channel[];
  selectedChannelId: string | null;
  videos: Video[];
  currentlyPlaying: Video | null;
  playlists: Playlist[];
  selectedPlaylistId: string | null;
  viewMode: "grid" | "list";
  showSidebar: boolean;
  isPlaying: boolean;
  volume: number;
  muted: boolean;
  fullscreen: boolean;
  currentTime: number;
  duration: number;

  // HLS & Streaming features
  hlsQuality: "auto" | "1080p" | "720p" | "480p";
  subtitlesEnabled: boolean;
  resumePromptTime: number | null;
  showUploadModal: boolean;

  setChannels: (channels: Channel[]) => void;
  setSelectedChannelId: (id: string | null) => void;
  setVideos: (videos: Video[]) => void;
  setCurrentlyPlaying: (video: Video | null) => void;
  setPlaylists: (playlists: Playlist[]) => void;
  setSelectedPlaylistId: (id: string | null) => void;
  setViewMode: (mode: "grid" | "list") => void;
  setShowSidebar: (show: boolean) => void;

  setHlsQuality: (quality: "auto" | "1080p" | "720p" | "480p") => void;
  setSubtitlesEnabled: (enabled: boolean) => void;
  setResumePromptTime: (time: number | null) => void;
  setShowUploadModal: (show: boolean) => void;

  play: () => void;
  pause: () => void;
  seek: (time: number) => void;
  setVolume: (vol: number) => void;
  toggleMute: () => void;
  toggleFullscreen: () => void;
  setCurrentTime: (t: number) => void;
  setDuration: (d: number) => void;
  setIsPlaying: (p: boolean) => void;
  saveProgress: (videoId: string, time: number) => void;
  addNewVideo: (video: Video) => void;
}

const INITIAL_CHANNELS: Channel[] = [
  {
    id: "channel-nexus-media",
    name: "Nexus Engineering",
    description: "Deep dive system architecture and distributed sovereign runtimes",
    subscriber_count: 14200,
    video_count: 4,
  },
  {
    id: "channel-creative",
    name: "Autonomous Studio",
    description: "Creative media production, high dynamic range rendering, and generative UI",
    subscriber_count: 8900,
    video_count: 2,
  },
];

const INITIAL_VIDEOS: Video[] = [
  {
    id: "video-distributed-cloud",
    channel_id: "channel-nexus-media",
    channel_name: "Nexus Engineering",
    title: "Inside the Sovereign Distributed Mesh",
    description: "Architecture breakdown of SQLite replication, WebSocket multi-leader presence, and local edge computing.",
    url: "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4",
    hls_url: "/api/v1/video/videos/video-distributed-cloud/hls/master.m3u8",
    thumbnail_url: "https://images.unsplash.com/photo-1518770660439-4636190af475?auto=format&fit=crop&w=800&q=80",
    duration: 596,
    width: 1920,
    height: 1080,
    views: 4520,
    saved_progress: 142,
    chapters: [
      { time: 0, title: "1. Mesh Foundations" },
      { time: 95, title: "2. SQLite Edge Replication" },
      { time: 280, title: "3. HLS Multicast Streaming" },
      { time: 460, title: "4. Sovereign Identity & Gateways" },
    ],
    subtitles: [
      { srclang: "en", label: "English CC", url: "/api/v1/video/videos/video-distributed-cloud/subtitles.vtt" },
    ],
    created_at: new Date(Date.now() - 86400000 * 3).toISOString(),
  },
  {
    id: "video-creative-suite",
    channel_id: "channel-creative",
    channel_name: "Autonomous Studio",
    title: "Vulkan Hardware Acceleration in Browser Viewports",
    description: "Rendering high throughput shaders with zero latency memory transfers.",
    url: "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/ElephantsDream.mp4",
    hls_url: "/api/v1/video/videos/video-creative-suite/hls/master.m3u8",
    thumbnail_url: "https://images.unsplash.com/photo-1550745165-9bc0b252726f?auto=format&fit=crop&w=800&q=80",
    duration: 653,
    width: 1920,
    height: 1080,
    views: 2980,
    saved_progress: 0,
    chapters: [
      { time: 0, title: "1. Vulkan Pipeline Init" },
      { time: 180, title: "2. Compute Shaders" },
      { time: 390, title: "3. Memory Barriers" },
      { time: 540, title: "4. Real-world Benchmarks" },
    ],
    subtitles: [
      { srclang: "en", label: "English CC", url: "/api/v1/video/videos/video-creative-suite/subtitles.vtt" },
    ],
    created_at: new Date(Date.now() - 86400000 * 1).toISOString(),
  },
];

export const useEditorStore = create<EditorStore>((set, get) => ({
  channels: INITIAL_CHANNELS,
  selectedChannelId: null,
  videos: INITIAL_VIDEOS,
  currentlyPlaying: INITIAL_VIDEOS[0],
  playlists: [],
  selectedPlaylistId: null,
  viewMode: "grid",
  showSidebar: true,
  isPlaying: false,
  volume: 1,
  muted: false,
  fullscreen: false,
  currentTime: 0,
  duration: 0,

  hlsQuality: "auto",
  subtitlesEnabled: true,
  resumePromptTime: INITIAL_VIDEOS[0].saved_progress || null,
  showUploadModal: false,

  setChannels: (channels) => set({ channels }),
  setSelectedChannelId: (id) => set({ selectedChannelId: id }),
  setVideos: (videos) => set({ videos }),
  setCurrentlyPlaying: (video) =>
    set({
      currentlyPlaying: video,
      isPlaying: !!video,
      resumePromptTime: video?.saved_progress && video.saved_progress > 10 ? video.saved_progress : null,
    }),
  setPlaylists: (playlists) => set({ playlists }),
  setSelectedPlaylistId: (id) => set({ selectedPlaylistId: id }),
  setViewMode: (mode) => set({ viewMode: mode }),
  setShowSidebar: (show) => set({ showSidebar: show }),

  setHlsQuality: (quality) => set({ hlsQuality: quality }),
  setSubtitlesEnabled: (enabled) => set({ subtitlesEnabled: enabled }),
  setResumePromptTime: (time) => set({ resumePromptTime: time }),
  setShowUploadModal: (show) => set({ showUploadModal: show }),

  play: () => set({ isPlaying: true }),
  pause: () => set({ isPlaying: false }),
  seek: (time) => set({ currentTime: time }),
  setVolume: (vol) => set({ volume: vol }),
  toggleMute: () => set((s) => ({ muted: !s.muted })),
  toggleFullscreen: () => set((s) => ({ fullscreen: !s.fullscreen })),
  setCurrentTime: (t) => set({ currentTime: t }),
  setDuration: (d) => set({ duration: d }),
  setIsPlaying: (p) => set({ isPlaying: p }),

  saveProgress: (videoId, time) => {
    set((s) => ({
      videos: s.videos.map((v) => (v.id === videoId ? { ...v, saved_progress: time } : v)),
    }));
    fetch(`/api/v1/video/videos/${videoId}/progress`, {
      method: "PUT",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ position: time, duration: get().duration }),
    }).catch(() => {});
  },

  addNewVideo: (video) =>
    set((s) => ({
      videos: [video, ...s.videos],
      currentlyPlaying: video,
      showUploadModal: false,
    })),
}));
