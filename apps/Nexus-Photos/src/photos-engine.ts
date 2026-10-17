import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
export interface Photo {
  id: string;
  title: string;
  url: string;
  albumId: string;
  tags: string;
  width: number;
  height: number;
  fileSize: number;
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
      "CREATE TABLE IF NOT EXISTS photos (id TEXT PRIMARY KEY, title TEXT, url TEXT, album_id TEXT, tags TEXT, width INTEGER, height INTEGER, file_size INTEGER, created_at TEXT)",
    );
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS albums (id TEXT PRIMARY KEY, name TEXT, description TEXT, photo_count INTEGER DEFAULT 0, created_at TEXT)",
    );
  }
  addPhoto(
    title: string,
    url: string,
    albumId: string,
    tags = "",
    width = 0,
    height = 0,
    fileSize = 0,
  ): Photo {
    const p: Photo = {
      id: randomUUID(),
      title,
      url,
      albumId,
      tags,
      width,
      height,
      fileSize,
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO photos VALUES (?,?,?,?,?,?,?,?,?)")
      .run(p.id, p.title, p.url, p.albumId, p.tags, p.width, p.height, p.fileSize, p.createdAt);
    this.db.prepare("UPDATE albums SET photo_count = photo_count + 1 WHERE id = ?").run(albumId);
    return p;
  }
  listPhotos(albumId?: string): Photo[] {
    if (albumId)
      return this.db
        .prepare("SELECT * FROM photos WHERE album_id = ? ORDER BY created_at DESC")
        .all(albumId) as Photo[];
    return this.db.prepare("SELECT * FROM photos ORDER BY created_at DESC").all() as Photo[];
  }
  getPhoto(id: string): Photo | undefined {
    return this.db.prepare("SELECT * FROM photos WHERE id = ?").get(id) as Photo | undefined;
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
      .prepare("INSERT INTO albums VALUES (?,?,?,?,?)")
      .run(a.id, a.name, a.description, a.photoCount, a.createdAt);
    return a;
  }
  listAlbums(): Album[] {
    return this.db.prepare("SELECT * FROM albums ORDER BY name ASC").all() as Album[];
  }
}
