import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface ChapterCue {
  time: number;
  title: string;
}

export interface SubtitleTrack {
  srclang: string;
  label: string;
  url: string;
}

export interface Video {
  id: string;
  title: string;
  description: string;
  url: string;
  hlsUrl: string;
  duration: number;
  channelId: string;
  tags: string;
  views: number;
  chapters: ChapterCue[];
  subtitles: SubtitleTrack[];
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

export interface PlaybackProgress {
  videoId: string;
  userId: string;
  positionSeconds: number;
  durationSeconds: number;
  completed: boolean;
  updatedAt: string;
}

export interface UploadSession {
  uploadId: string;
  filename: string;
  totalChunks: number;
  uploadedChunks: number[];
  fileSize: number;
  status: "uploading" | "assembling" | "transcoding" | "ready";
  createdAt: string;
}

export class VideoEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      `CREATE TABLE IF NOT EXISTS videos (
        id TEXT PRIMARY KEY,
        title TEXT,
        description TEXT,
        url TEXT,
        duration REAL,
        channel_id TEXT,
        tags TEXT,
        views INTEGER DEFAULT 0,
        created_at TEXT,
        hls_url TEXT DEFAULT '',
        chapters_json TEXT DEFAULT '[]',
        subtitles_json TEXT DEFAULT '[]'
      )`,
    );
    this.db.exec(
      `CREATE TABLE IF NOT EXISTS channels (
        id TEXT PRIMARY KEY,
        name TEXT,
        description TEXT,
        video_count INTEGER DEFAULT 0,
        subscriber_count INTEGER DEFAULT 0,
        created_at TEXT
      )`,
    );
    this.db.exec(
      `CREATE TABLE IF NOT EXISTS video_playlists (
        id TEXT PRIMARY KEY,
        name TEXT,
        description TEXT,
        video_count INTEGER DEFAULT 0,
        created_at TEXT
      )`,
    );
    this.db.exec(
      `CREATE TABLE IF NOT EXISTS playback_history (
        video_id TEXT,
        user_id TEXT,
        position_seconds REAL,
        duration_seconds REAL,
        completed INTEGER DEFAULT 0,
        updated_at TEXT,
        PRIMARY KEY (video_id, user_id)
      )`,
    );
    this.db.exec(
      `CREATE TABLE IF NOT EXISTS upload_sessions (
        upload_id TEXT PRIMARY KEY,
        filename TEXT,
        total_chunks INTEGER,
        uploaded_chunks_json TEXT DEFAULT '[]',
        file_size INTEGER,
        status TEXT,
        created_at TEXT
      )`,
    );

    // Schema migrations for existing databases
    try {
      this.db.exec("ALTER TABLE videos ADD COLUMN hls_url TEXT DEFAULT ''");
    } catch {}
    try {
      this.db.exec("ALTER TABLE videos ADD COLUMN chapters_json TEXT DEFAULT '[]'");
    } catch {}
    try {
      this.db.exec("ALTER TABLE videos ADD COLUMN subtitles_json TEXT DEFAULT '[]'");
    } catch {}
  }

  private mapVideo(row: any): Video {
    let chapters: ChapterCue[] = [];
    let subtitles: SubtitleTrack[] = [];
    try {
      chapters = row.chapters_json ? JSON.parse(row.chapters_json) : [];
    } catch {
      chapters = [];
    }
    try {
      subtitles = row.subtitles_json ? JSON.parse(row.subtitles_json) : [];
    } catch {
      subtitles = [];
    }

    return {
      id: row.id,
      title: row.title,
      description: row.description || "",
      url: row.url,
      hlsUrl: row.hls_url || `/api/v1/video/videos/${row.id}/hls/master.m3u8`,
      duration: Number(row.duration || 0),
      channelId: row.channel_id || "",
      tags: row.tags || "",
      views: Number(row.views || 0),
      chapters,
      subtitles,
      createdAt: row.created_at,
    };
  }

  addVideo(
    title: string,
    description: string,
    url: string,
    duration: number,
    channelId: string,
    tags = "",
    customChapters?: ChapterCue[],
    customSubtitles?: SubtitleTrack[],
  ): Video {
    const id = randomUUID();
    const createdAt = new Date().toISOString();
    const hlsUrl = `/api/v1/video/videos/${id}/hls/master.m3u8`;

    const resolvedChapters: ChapterCue[] = customChapters || [
      { time: 0, title: "Introduction" },
      { time: Math.floor(duration * 0.25), title: "Overview & Setup" },
      { time: Math.floor(duration * 0.6), title: "Deep Dive Analysis" },
      { time: Math.floor(duration * 0.85), title: "Conclusion & Summary" },
    ];

    const resolvedSubtitles: SubtitleTrack[] = customSubtitles || [
      { srclang: "en", label: "English", url: `/api/v1/video/videos/${id}/subtitles.vtt` },
    ];

    const chaptersJson = JSON.stringify(resolvedChapters);
    const subtitlesJson = JSON.stringify(resolvedSubtitles);

    this.db
      .prepare(
        `INSERT INTO videos (id, title, description, url, duration, channel_id, tags, views, created_at, hls_url, chapters_json, subtitles_json)
         VALUES (?, ?, ?, ?, ?, ?, ?, 0, ?, ?, ?, ?)`,
      )
      .run(
        id,
        title,
        description,
        url,
        duration,
        channelId,
        tags,
        createdAt,
        hlsUrl,
        chaptersJson,
        subtitlesJson,
      );

    this.db.prepare("UPDATE channels SET video_count = video_count + 1 WHERE id = ?").run(channelId);

    return {
      id,
      title,
      description,
      url,
      hlsUrl,
      duration,
      channelId,
      tags,
      views: 0,
      chapters: resolvedChapters,
      subtitles: resolvedSubtitles,
      createdAt,
    };
  }

  listVideos(channelId?: string): Video[] {
    if (channelId) {
      const rows = this.db
        .prepare("SELECT * FROM videos WHERE channel_id = ? ORDER BY created_at DESC")
        .all(channelId) as any[];
      return rows.map((r) => this.mapVideo(r));
    }
    const rows = this.db.prepare("SELECT * FROM videos ORDER BY created_at DESC").all() as any[];
    return rows.map((r) => this.mapVideo(r));
  }

  getVideo(id: string): Video | undefined {
    const row = this.db.prepare("SELECT * FROM videos WHERE id = ?").get(id) as any;
    return row ? this.mapVideo(row) : undefined;
  }

  generateHlsMasterPlaylist(videoId: string): string {
    return `#EXTM3U
#EXT-X-VERSION:4
#EXT-X-INDEPENDENT-SEGMENTS

#EXT-X-STREAM-INF:BANDWIDTH=5000000,AVERAGE-BANDWIDTH=4500000,RESOLUTION=1920x1080,FRAME-RATE=60.000,CODECS="avc1.64002a,mp4a.40.2"
/api/v1/video/videos/${videoId}/hls/1080p.m3u8

#EXT-X-STREAM-INF:BANDWIDTH=2800000,AVERAGE-BANDWIDTH=2500000,RESOLUTION=1280x720,FRAME-RATE=30.000,CODECS="avc1.4d401f,mp4a.40.2"
/api/v1/video/videos/${videoId}/hls/720p.m3u8

#EXT-X-STREAM-INF:BANDWIDTH=1200000,AVERAGE-BANDWIDTH=1000000,RESOLUTION=854x480,FRAME-RATE=30.000,CODECS="avc1.4d401e,mp4a.40.2"
/api/v1/video/videos/${videoId}/hls/480p.m3u8
`;
  }

  generateWebVttSubtitles(videoId: string): string {
    const video = this.getVideo(videoId);
    const title = video ? video.title : "Nexus Video";

    return `WEBVTT - Nexus Sovereign Stream Subtitles

00:00:01.000 --> 00:00:04.500
[Narrator]: Welcome to ${title}.

00:00:05.000 --> 00:00:09.500
[Narrator]: Sovereign media streaming running on Nexus platform infrastructure.

00:00:10.000 --> 00:00:15.000
[Music]: High fidelity audio and adaptive bitrate HLS transmission.
`;
  }

  savePlaybackPosition(
    videoId: string,
    userId: string,
    positionSeconds: number,
    durationSeconds: number,
  ): PlaybackProgress {
    const completed = durationSeconds > 0 && positionSeconds >= durationSeconds * 0.95 ? 1 : 0;
    const updatedAt = new Date().toISOString();

    this.db
      .prepare(
        `INSERT INTO playback_history (video_id, user_id, position_seconds, duration_seconds, completed, updated_at)
         VALUES (?, ?, ?, ?, ?, ?)
         ON CONFLICT(video_id, user_id) DO UPDATE SET
           position_seconds = excluded.position_seconds,
           duration_seconds = excluded.duration_seconds,
           completed = excluded.completed,
           updated_at = excluded.updated_at`,
      )
      .run(videoId, userId, positionSeconds, durationSeconds, completed, updatedAt);

    return {
      videoId,
      userId,
      positionSeconds,
      durationSeconds,
      completed: Boolean(completed),
      updatedAt,
    };
  }

  getPlaybackPosition(videoId: string, userId: string): PlaybackProgress | undefined {
    const row = this.db
      .prepare("SELECT * FROM playback_history WHERE video_id = ? AND user_id = ?")
      .get(videoId, userId) as any;

    if (!row) return undefined;

    return {
      videoId: row.video_id,
      userId: row.user_id,
      positionSeconds: Number(row.position_seconds || 0),
      durationSeconds: Number(row.duration_seconds || 0),
      completed: Boolean(row.completed),
      updatedAt: row.updated_at,
    };
  }

  initChunkedUpload(filename: string, totalChunks: number, fileSize: number): UploadSession {
    const uploadId = randomUUID();
    const createdAt = new Date().toISOString();
    const status = "uploading";

    this.db
      .prepare(
        `INSERT INTO upload_sessions (upload_id, filename, total_chunks, uploaded_chunks_json, file_size, status, created_at)
         VALUES (?, ?, ?, '[]', ?, ?, ?)`,
      )
      .run(uploadId, filename, totalChunks, fileSize, status, createdAt);

    return {
      uploadId,
      filename,
      totalChunks,
      uploadedChunks: [],
      fileSize,
      status,
      createdAt,
    };
  }

  recordChunk(uploadId: string, chunkIndex: number): UploadSession {
    const row = this.db.prepare("SELECT * FROM upload_sessions WHERE upload_id = ?").get(uploadId) as any;
    if (!row) throw new Error("Upload session not found");

    let uploaded: number[] = [];
    try {
      uploaded = JSON.parse(row.uploaded_chunks_json || "[]");
    } catch {
      uploaded = [];
    }

    if (!uploaded.includes(chunkIndex)) {
      uploaded.push(chunkIndex);
      uploaded.sort((a, b) => a - b);
    }

    const isComplete = uploaded.length >= row.total_chunks;
    const status = isComplete ? "transcoding" : "uploading";

    this.db
      .prepare("UPDATE upload_sessions SET uploaded_chunks_json = ?, status = ? WHERE upload_id = ?")
      .run(JSON.stringify(uploaded), status, uploadId);

    return {
      uploadId: row.upload_id,
      filename: row.filename,
      totalChunks: Number(row.total_chunks),
      uploadedChunks: uploaded,
      fileSize: Number(row.file_size),
      status,
      createdAt: row.created_at,
    };
  }

  completeChunkedUpload(
    uploadId: string,
    title: string,
    channelId: string,
    description = "",
  ): Video {
    const row = this.db.prepare("SELECT * FROM upload_sessions WHERE upload_id = ?").get(uploadId) as any;
    if (!row) throw new Error("Upload session not found");

    this.db.prepare("UPDATE upload_sessions SET status = 'ready' WHERE upload_id = ?").run(uploadId);

    const videoUrl = `/api/v1/video/stream/${uploadId}/${encodeURIComponent(row.filename)}`;
    return this.addVideo(title, description, videoUrl, 180, channelId, "chunked-upload,hls");
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
    return this.db.prepare("SELECT * FROM video_playlists ORDER BY name ASC").all() as VideoPlaylist[];
  }
}
