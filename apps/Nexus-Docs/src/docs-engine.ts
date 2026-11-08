import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface Document {
  id: string;
  title: string;
  content: string;
  folder: string;
  icon: string;
  coverUrl: string;
  pinned: number;
  starred: number;
  createdAt: string;
  updatedAt: string;
}

export interface Folder {
  id: string;
  name: string;
  color: string;
  icon: string;
  createdAt: string;
}

export interface DocumentRevision {
  id: string;
  documentId: string;
  title: string;
  content: string;
  createdAt: string;
}

// Backwards-compatible alias
export interface Docs {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class DocsEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    // Legacy table
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS docses (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)"
    );
    // Modern Documents table
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS documents (id TEXT PRIMARY KEY, title TEXT, content TEXT, folder TEXT, icon TEXT, cover_url TEXT, pinned INTEGER DEFAULT 0, starred INTEGER DEFAULT 0, created_at TEXT, updated_at TEXT)"
    );
    // Folders table
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS folders (id TEXT PRIMARY KEY, name TEXT, color TEXT, icon TEXT, created_at TEXT)"
    );
    // Document Revisions table
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS document_revisions (id TEXT PRIMARY KEY, document_id TEXT, title TEXT, content TEXT, created_at TEXT)"
    );
  }

  // ── Document Operations ──

  createDocument(
    title: string,
    content = "",
    folder = "General",
    icon = "📄",
    coverUrl = ""
  ): Document {
    const doc: Document = {
      id: randomUUID(),
      title,
      content,
      folder,
      icon,
      coverUrl,
      pinned: 0,
      starred: 0,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO documents VALUES (?,?,?,?,?,?,?,?,?,?)"
      )
      .run(
        doc.id,
        doc.title,
        doc.content,
        doc.folder,
        doc.icon,
        doc.coverUrl,
        doc.pinned,
        doc.starred,
        doc.createdAt,
        doc.updatedAt
      );

    // Keep legacy table synced
    this.createDocs(title, folder);

    // Initial revision snapshot
    this.createRevision(doc.id, doc.title, doc.content);

    return doc;
  }

  listDocuments(folder?: string): Document[] {
    if (folder) {
      return this.db
        .prepare(
          "SELECT id, title, content, folder, icon, cover_url as coverUrl, pinned, starred, created_at as createdAt, updated_at as updatedAt FROM documents WHERE folder = ? ORDER BY pinned DESC, updated_at DESC"
        )
        .all(folder) as Document[];
    }
    return this.db
      .prepare(
        "SELECT id, title, content, folder, icon, cover_url as coverUrl, pinned, starred, created_at as createdAt, updated_at as updatedAt FROM documents ORDER BY pinned DESC, updated_at DESC"
      )
      .all() as Document[];
  }

  getDocument(id: string): Document | undefined {
    return this.db
      .prepare(
        "SELECT id, title, content, folder, icon, cover_url as coverUrl, pinned, starred, created_at as createdAt, updated_at as updatedAt FROM documents WHERE id = ?"
      )
      .get(id) as Document | undefined;
  }

  updateDocument(
    id: string,
    updates: {
      title?: string;
      content?: string;
      folder?: string;
      icon?: string;
      coverUrl?: string;
      pinned?: number;
      starred?: number;
    }
  ): Document | undefined {
    const existing = this.getDocument(id);
    if (!existing) return undefined;

    const title = updates.title !== undefined ? updates.title : existing.title;
    const content = updates.content !== undefined ? updates.content : existing.content;
    const folder = updates.folder !== undefined ? updates.folder : existing.folder;
    const icon = updates.icon !== undefined ? updates.icon : existing.icon;
    const coverUrl = updates.coverUrl !== undefined ? updates.coverUrl : existing.coverUrl;
    const pinned = updates.pinned !== undefined ? updates.pinned : existing.pinned;
    const starred = updates.starred !== undefined ? updates.starred : existing.starred;
    const updatedAt = new Date().toISOString();

    this.db
      .prepare(
        "UPDATE documents SET title = ?, content = ?, folder = ?, icon = ?, cover_url = ?, pinned = ?, starred = ?, updated_at = ? WHERE id = ?"
      )
      .run(title, content, folder, icon, coverUrl, pinned, starred, updatedAt, id);

    return this.getDocument(id);
  }

  deleteDocument(id: string): boolean {
    const existing = this.getDocument(id);
    if (!existing) return false;
    this.db.prepare("DELETE FROM documents WHERE id = ?").run(id);
    return true;
  }

  searchDocuments(query: string): Document[] {
    return this.db
      .prepare(
        "SELECT id, title, content, folder, icon, cover_url as coverUrl, pinned, starred, created_at as createdAt, updated_at as updatedAt FROM documents WHERE title LIKE ? OR content LIKE ? ORDER BY updated_at DESC"
      )
      .all(`%${query}%`, `%${query}%`) as Document[];
  }

  // ── Document Revisions ──

  createRevision(
    documentId: string,
    title?: string,
    content?: string
  ): DocumentRevision {
    const doc = this.getDocument(documentId);
    const rev: DocumentRevision = {
      id: randomUUID(),
      documentId,
      title: title !== undefined ? title : (doc?.title || ""),
      content: content !== undefined ? content : (doc?.content || ""),
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO document_revisions VALUES (?,?,?,?,?)"
      )
      .run(rev.id, rev.documentId, rev.title, rev.content, rev.createdAt);
    return rev;
  }

  listRevisions(documentId: string): DocumentRevision[] {
    return this.db
      .prepare(
        "SELECT id, document_id as documentId, title, content, created_at as createdAt FROM document_revisions WHERE document_id = ? ORDER BY created_at DESC"
      )
      .all(documentId) as DocumentRevision[];
  }

  restoreRevision(documentId: string, revisionId: string): Document | undefined {
    const rev = this.db
      .prepare(
        "SELECT id, document_id as documentId, title, content, created_at as createdAt FROM document_revisions WHERE id = ? AND document_id = ?"
      )
      .get(revisionId, documentId) as DocumentRevision | undefined;
    if (!rev) return undefined;

    const current = this.getDocument(documentId);
    if (current) {
      this.createRevision(documentId, current.title, current.content);
    }

    return this.updateDocument(documentId, {
      title: rev.title,
      content: rev.content,
    });
  }

  // ── Folders ──

  createFolder(name: string, color = "#6366f1", icon = "📁"): Folder {
    const folder: Folder = {
      id: randomUUID(),
      name,
      color,
      icon,
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO folders VALUES (?,?,?,?,?)")
      .run(folder.id, folder.name, folder.color, folder.icon, folder.createdAt);
    return folder;
  }

  listFolders(): Folder[] {
    return this.db
      .prepare("SELECT id, name, color, icon, created_at as createdAt FROM folders ORDER BY name ASC")
      .all() as Folder[];
  }

  // ── Legacy Compatibility ──

  createDocs(name: string, type = ""): Docs {
    const d: Docs = {
      id: randomUUID(),
      name,
      description: "",
      type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO docses VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listDocss(): Docs[] {
    return this.db.prepare("SELECT * FROM docses ORDER BY updated_at DESC").all() as Docs[];
  }

  getDocs(id: string): Docs | undefined {
    return this.db.prepare("SELECT * FROM docses WHERE id = ?").get(id) as Docs | undefined;
  }
}
