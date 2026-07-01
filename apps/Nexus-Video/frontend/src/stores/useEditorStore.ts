import { create } from "zustand";

export interface Channel {
  id: string;
  name: string;
  description: string;
  avatar_url: string;
  banner_url: string;
  subscriber_count: number;
  video_count: number;
  is_verified: boolean;
}

export interface Video {
  id: string;
  channel_id: string;
  title: string;
  description: string;
  url: string;
  thumbnail_url: string;
  duration: number;
  width: number;
  height: number;
  file_size: number;
  content_type: string;
  tags: string;
  views: number;
  likes: number;
  is_published: boolean;
  status: string;
  channel_name?: string;
  created_at: string;
}

export interface Playlist {
  id: string;
  name: string;
  description: string;
  channel_id: string;
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

  setChannels: (channels: Channel[]) => void;
  setSelectedChannelId: (id: string | null) => void;
  setVideos: (videos: Video[]) => void;
  setCurrentlyPlaying: (video: Video | null) => void;
  setPlaylists: (playlists: Playlist[]) => void;
  setSelectedPlaylistId: (id: string | null) => void;
  setViewMode: (mode: "grid" | "list") => void;
  setShowSidebar: (show: boolean) => void;

  play: () => void;
  pause: () => void;
  seek: (time: number) => void;
  setVolume: (vol: number) => void;
  toggleMute: () => void;
  toggleFullscreen: () => void;
  setCurrentTime: (t: number) => void;
  setDuration: (d: number) => void;
  setIsPlaying: (p: boolean) => void;
}

export const useEditorStore = create<EditorStore>((set) => ({
  channels: [],
  selectedChannelId: null,
  videos: [],
  currentlyPlaying: null,
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

  setChannels: (channels) => set({ channels }),
  setSelectedChannelId: (id) => set({ selectedChannelId: id }),
  setVideos: (videos) => set({ videos }),
  setCurrentlyPlaying: (video) => set({ currentlyPlaying: video, isPlaying: !!video }),
  setPlaylists: (playlists) => set({ playlists }),
  setSelectedPlaylistId: (id) => set({ selectedPlaylistId: id }),
  setViewMode: (mode) => set({ viewMode: mode }),
  setShowSidebar: (show) => set({ showSidebar: show }),

  play: () => set({ isPlaying: true }),
  pause: () => set({ isPlaying: false }),
  seek: (time) => set({ currentTime: time }),
  setVolume: (vol) => set({ volume: Math.max(0, Math.min(1, vol)), muted: vol === 0 }),
  toggleMute: () => set((s) => ({ muted: !s.muted })),
  toggleFullscreen: () => set((s) => ({ fullscreen: !s.fullscreen })),
  setCurrentTime: (t) => set({ currentTime: t }),
  setDuration: (d) => set({ duration: d }),
  setIsPlaying: (p) => set({ isPlaying: p }),
}));
