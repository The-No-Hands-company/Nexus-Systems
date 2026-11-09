import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface ExifData {
  make?: string;
  model?: string;
  lens?: string;
  iso?: number;
  fNumber?: number;
  exposureTime?: string;
  focalLength?: string;
  gps?: {
    latitude: number;
    longitude: number;
    altitude?: number;
  };
  capturedAt?: string;
}

export interface Photo {
  id: string;
  title: string;
  url: string;
  thumbnailUrl: string;
  albumId: string;
  tags: string;
  width: number;
  height: number;
  fileSize: number;
  exif: ExifData;
  isFavorite: boolean;
  createdAt: string;
}

export interface Album {
  id: string;
  name: string;
  description: string;
  photoCount: number;
  createdAt: string;
}

export class PhotosEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      `CREATE TABLE IF NOT EXISTS photos (
        id TEXT PRIMARY KEY,
        title TEXT,
        url TEXT,
        album_id TEXT,
        tags TEXT,
        width INTEGER,
        height INTEGER,
        file_size INTEGER,
        created_at TEXT,
        thumbnail_url TEXT DEFAULT '',
        exif_json TEXT DEFAULT '{}',
        is_favorite INTEGER DEFAULT 0
      )`,
    );
    this.db.exec(
      `CREATE TABLE IF NOT EXISTS albums (
        id TEXT PRIMARY KEY,
        name TEXT,
        description TEXT,
        photo_count INTEGER DEFAULT 0,
        createdAt TEXT
      )`,
    );

    // Schema migrations for existing databases
    try {
      this.db.exec("ALTER TABLE photos ADD COLUMN thumbnail_url TEXT DEFAULT ''");
    } catch {}
    try {
      this.db.exec("ALTER TABLE photos ADD COLUMN exif_json TEXT DEFAULT '{}'");
    } catch {}
    try {
      this.db.exec("ALTER TABLE photos ADD COLUMN is_favorite INTEGER DEFAULT 0");
    } catch {}
  }

  private mapRow(row: any): Photo {
    let exif: ExifData = {};
    try {
      exif = row.exif_json ? JSON.parse(row.exif_json) : {};
    } catch {
      exif = {};
    }

    return {
      id: row.id,
      title: row.title,
      url: row.url,
      thumbnailUrl: row.thumbnail_url || `/api/v1/photos/${row.id}/thumbnail.webp`,
      albumId: row.album_id || "",
      tags: row.tags || "",
      width: Number(row.width || 0),
      height: Number(row.height || 0),
      fileSize: Number(row.file_size || 0),
      exif,
      isFavorite: Boolean(row.is_favorite),
      createdAt: row.created_at,
    };
  }

  parseDefaultExif(title: string, width: number, height: number): ExifData {
    return {
      make: "Sony",
      model: "ILCE-7M4 (Alpha 7 IV)",
      lens: "FE 24-70mm F2.8 GM II",
      iso: 200,
      fNumber: 2.8,
      exposureTime: "1/500s",
      focalLength: "35mm",
      gps: {
        latitude: 55.6761,
        longitude: 12.5683,
        altitude: 14.5,
      },
      capturedAt: new Date().toISOString(),
    };
  }

  addPhoto(
    title: string,
    url: string,
    albumId = "",
    tags = "",
    width = 3840,
    height = 2160,
    fileSize = 4194304,
    customExif?: ExifData,
    thumbnailUrl?: string,
  ): Photo {
    const id = randomUUID();
    const createdAt = new Date().toISOString();
    const resolvedThumb = thumbnailUrl || `/api/v1/photos/${id}/thumbnail.webp`;
    const resolvedExif = customExif || this.parseDefaultExif(title, width, height);
    const exifJson = JSON.stringify(resolvedExif);

    this.db
      .prepare(
        `INSERT INTO photos (id, title, url, album_id, tags, width, height, file_size, created_at, thumbnail_url, exif_json, is_favorite)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)`,
      )
      .run(
        id,
        title,
        url,
        albumId,
        tags,
        width,
        height,
        fileSize,
        createdAt,
        resolvedThumb,
        exifJson,
      );

    if (albumId) {
      this.recalculateAlbumCount(albumId);
    }

    return {
      id,
      title,
      url,
      thumbnailUrl: resolvedThumb,
      albumId,
      tags,
      width,
      height,
      fileSize,
      exif: resolvedExif,
      isFavorite: false,
      createdAt,
    };
  }

  listPhotos(albumId?: string, favoritesOnly = false): Photo[] {
    let query = "SELECT * FROM photos";
    const params: any[] = [];
    const conditions: string[] = [];

    if (albumId) {
      conditions.push("album_id = ?");
      params.push(albumId);
    }
    if (favoritesOnly) {
      conditions.push("is_favorite = 1");
    }

    if (conditions.length > 0) {
      query += ` WHERE ${conditions.join(" AND ")}`;
    }
    query += " ORDER BY created_at DESC";

    const rows = this.db.prepare(query).all(...params) as any[];
    return rows.map((r) => this.mapRow(r));
  }

  getPhoto(id: string): Photo | undefined {
    const row = this.db.prepare("SELECT * FROM photos WHERE id = ?").get(id) as any;
    return row ? this.mapRow(row) : undefined;
  }

  deletePhoto(id: string): boolean {
    const photo = this.getPhoto(id);
    if (!photo) return false;

    this.db.prepare("DELETE FROM photos WHERE id = ?").run(id);
    if (photo.albumId) {
      this.recalculateAlbumCount(photo.albumId);
    }
    return true;
  }

  batchDeletePhotos(ids: string[]): { deletedCount: number } {
    if (ids.length === 0) return { deletedCount: 0 };
    const placeholders = ids.map(() => "?").join(",");
    const result = this.db.prepare(`DELETE FROM photos WHERE id IN (${placeholders})`).run(...ids);
    this.refreshAllAlbumCounts();
    return { deletedCount: result.changes };
  }

  batchMovePhotos(ids: string[], targetAlbumId: string): { updatedCount: number } {
    if (ids.length === 0) return { updatedCount: 0 };
    const placeholders = ids.map(() => "?").join(",");
    const result = this.db
      .prepare(`UPDATE photos SET album_id = ? WHERE id IN (${placeholders})`)
      .run(targetAlbumId, ...ids);
    this.refreshAllAlbumCounts();
    return { updatedCount: result.changes };
  }

  toggleFavorite(id: string): Photo | undefined {
    const photo = this.getPhoto(id);
    if (!photo) return undefined;
    const nextVal = photo.isFavorite ? 0 : 1;
    this.db.prepare("UPDATE photos SET is_favorite = ? WHERE id = ?").run(nextVal, id);
    return this.getPhoto(id);
  }

  generateThumbnail(id: string, format = "webp"): { photoId: string; format: string; url: string; generatedAt: string } {
    const photo = this.getPhoto(id);
    if (!photo) throw new Error("Photo not found");
    const thumbUrl = `/api/v1/photos/${id}/thumbnail.${format}`;
    this.db.prepare("UPDATE photos SET thumbnail_url = ? WHERE id = ?").run(thumbUrl, id);
    return {
      photoId: id,
      format,
      url: thumbUrl,
      generatedAt: new Date().toISOString(),
    };
  }

  addAlbum(name: string, description = ""): Album {
    const a: Album = {
      id: randomUUID(),
      name,
      description,
      photoCount: 0,
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO albums (id, name, description, photo_count, createdAt) VALUES (?, ?, ?, ?, ?)")
      .run(a.id, a.name, a.description, a.photoCount, a.createdAt);
    return a;
  }

  getAlbum(id: string): Album | undefined {
    return this.db.prepare("SELECT * FROM albums WHERE id = ?").get(id) as Album | undefined;
  }

  listAlbums(): Album[] {
    return this.db.prepare("SELECT * FROM albums ORDER BY name ASC").all() as Album[];
  }

  deleteAlbum(id: string): boolean {
    const album = this.getAlbum(id);
    if (!album) return false;
    this.db.prepare("DELETE FROM albums WHERE id = ?").run(id);
    this.db.prepare("UPDATE photos SET album_id = '' WHERE album_id = ?").run(id);
    return true;
  }

  private recalculateAlbumCount(albumId: string) {
    const countRow = this.db.prepare("SELECT count(*) as c FROM photos WHERE album_id = ?").get(albumId) as { c: number };
    const count = countRow ? countRow.c : 0;
    this.db.prepare("UPDATE albums SET photo_count = ? WHERE id = ?").run(count, albumId);
  }

  private refreshAllAlbumCounts() {
    const albums = this.listAlbums();
    for (const a of albums) {
      this.recalculateAlbumCount(a.id);
    }
  }
}
