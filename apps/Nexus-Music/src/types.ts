export interface Track {
  id: string;
  title: string;
  artist: string;
  album: string;
  duration: number;
  genre: string;
  url: string;
  createdAt: string;
}

export interface Playlist {
  id: string;
  name: string;
  description: string;
  trackCount: number;
  createdAt: string;
}

export interface PlaylistTrack {
  id: string;
  playlistId: string;
  trackId: string;
  order: number;
  addedAt: string;
}
