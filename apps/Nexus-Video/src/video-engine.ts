import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
export interface Video {
  id: string;
  title: string;
  description: string;
  url: string;
  duration: number;
  channelId: string;
  tags: string;
  views: number;
  createdAt: string;
}
export interface Channel {
  id: string;
  name: string;
  description: string;
  videoCount: number;
  subscriberCount: number;
  createdAt: string;
}
export interface VideoPlaylist {
  id: string;
  name: string;
  description: string;
  videoCount: number;
  createdAt: string;
}
export class VideoEngine {
  db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS videos (id TEXT PRIMARY KEY, title TEXT, description TEXT, url TEXT, duration REAL, channel_id TEXT, tags TEXT, views INTEGER DEFAULT 0, created_at TEXT)",
    );
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS channels (id TEXT PRIMARY KEY, name TEXT, description TEXT, video_count INTEGER DEFAULT 0, subscriber_count INTEGER DEFAULT 0, created_at TEXT)",
    );
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS video_playlists (id TEXT PRIMARY KEY, name TEXT, description TEXT, video_count INTEGER DEFAULT 0, created_at TEXT)",
    );
  }
  addVideo(
    title: string,
    description: string,
    url: string,
    duration: number,
    channelId: string,
    tags = "",
  ): Video {
    const v: Video = {
      id: randomUUID(),
      title,
      description,
      url,
      duration,
      channelId,
      tags,
      views: 0,
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO videos VALUES (?,?,?,?,?,?,?,?,?)")
      .run(
        v.id,
        v.title,
        v.description,
        v.url,
        v.duration,
        v.channelId,
        v.tags,
        v.views,
        v.createdAt,
      );
    this.db
      .prepare("UPDATE channels SET video_count = video_count + 1 WHERE id = ?")
      .run(channelId);
    return v;
  }
  listVideos(channelId?: string): Video[] {
    if (channelId)
      return this.db
        .prepare("SELECT * FROM videos WHERE channel_id = ? ORDER BY created_at DESC")
        .all(channelId) as Video[];
    return this.db.prepare("SELECT * FROM videos ORDER BY created_at DESC").all() as Video[];
  }
  getVideo(id: string): Video | undefined {
    return this.db.prepare("SELECT * FROM videos WHERE id = ?").get(id) as Video | undefined;
  }
  addChannel(name: string, description = ""): Channel {
    const c: Channel = {
      id: randomUUID(),
      name,
      description,
      videoCount: 0,
      subscriberCount: 0,
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO channels VALUES (?,?,?,?,?,?)")
      .run(c.id, c.name, c.description, c.videoCount, c.subscriberCount, c.createdAt);
    return c;
  }
  listChannels(): Channel[] {
    return this.db.prepare("SELECT * FROM channels ORDER BY name ASC").all() as Channel[];
  }
  addPlaylist(name: string, description = ""): VideoPlaylist {
    const pl: VideoPlaylist = {
      id: randomUUID(),
      name,
      description,
      videoCount: 0,
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO video_playlists VALUES (?,?,?,?,?)")
      .run(pl.id, pl.name, pl.description, pl.videoCount, pl.createdAt);
    return pl;
  }
  listPlaylists(): VideoPlaylist[] {
    return this.db
      .prepare("SELECT * FROM video_playlists ORDER BY name ASC")
      .all() as VideoPlaylist[];
  }
}
