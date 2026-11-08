import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface Note {
  id: string;
  title: string;
  content: string;
  notebook: string;
  tags: string;
  pinned: number;
  createdAt: string;
  updatedAt: string;
}

export interface Notebook {
  id: string;
  name: string;
  description: string;
  noteCount: number;
  createdAt: string;
}

export class NotesEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS notes (id TEXT PRIMARY KEY, title TEXT, content TEXT, notebook TEXT, tags TEXT, pinned INTEGER DEFAULT 0, created_at TEXT, updated_at TEXT)"
    );
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS notebooks (id TEXT PRIMARY KEY, name TEXT, description TEXT, note_count INTEGER DEFAULT 0, created_at TEXT)"
    );
  }

  addNote(title: string, content: string, notebook = "General", tags = ""): Note {
    const n: Note = {
      id: randomUUID(),
      title,
      content,
      notebook,
      tags,
      pinned: 0,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO notes VALUES (?,?,?,?,?,?,?,?)")
      .run(n.id, n.title, n.content, n.notebook, n.tags, n.pinned, n.createdAt, n.updatedAt);
    this.db.prepare("UPDATE notebooks SET note_count = note_count + 1 WHERE id = ?").run(notebook);
    return n;
  }

  listNotes(notebook?: string): Note[] {
    if (notebook) {
      return this.db
        .prepare("SELECT id, title, content, notebook, tags, pinned, created_at as createdAt, updated_at as updatedAt FROM notes WHERE notebook = ? ORDER BY pinned DESC, updated_at DESC")
        .all(notebook) as Note[];
    }
    return this.db
      .prepare("SELECT id, title, content, notebook, tags, pinned, created_at as createdAt, updated_at as updatedAt FROM notes ORDER BY pinned DESC, updated_at DESC")
      .all() as Note[];
  }

  getNote(id: string): Note | undefined {
    return this.db
      .prepare("SELECT id, title, content, notebook, tags, pinned, created_at as createdAt, updated_at as updatedAt FROM notes WHERE id = ?")
      .get(id) as Note | undefined;
  }

  updateNote(
    id: string,
    updates: { title?: string; content?: string; notebook?: string; tags?: string; pinned?: number }
  ): Note | undefined {
    const existing = this.getNote(id);
    if (!existing) return undefined;

    const title = updates.title !== undefined ? updates.title : existing.title;
    const content = updates.content !== undefined ? updates.content : existing.content;
    const notebook = updates.notebook !== undefined ? updates.notebook : existing.notebook;
    const tags = updates.tags !== undefined ? updates.tags : existing.tags;
    const pinned = updates.pinned !== undefined ? updates.pinned : existing.pinned;
    const updatedAt = new Date().toISOString();

    this.db
      .prepare(
        "UPDATE notes SET title = ?, content = ?, notebook = ?, tags = ?, pinned = ?, updated_at = ? WHERE id = ?"
      )
      .run(title, content, notebook, tags, pinned, updatedAt, id);

    if (existing.notebook !== notebook) {
      this.db.prepare("UPDATE notebooks SET note_count = MAX(0, note_count - 1) WHERE id = ?").run(existing.notebook);
      this.db.prepare("UPDATE notebooks SET note_count = note_count + 1 WHERE id = ?").run(notebook);
    }

    return this.getNote(id);
  }

  deleteNote(id: string): boolean {
    const existing = this.getNote(id);
    if (!existing) return false;

    this.db.prepare("DELETE FROM notes WHERE id = ?").run(id);
    this.db.prepare("UPDATE notebooks SET note_count = MAX(0, note_count - 1) WHERE id = ?").run(existing.notebook);
    return true;
  }

  togglePin(id: string): Note | undefined {
    const existing = this.getNote(id);
    if (!existing) return undefined;
    const newPinned = existing.pinned ? 0 : 1;
    return this.updateNote(id, { pinned: newPinned });
  }

  searchNotes(q: string): Note[] {
    return this.db
      .prepare(
        "SELECT id, title, content, notebook, tags, pinned, created_at as createdAt, updated_at as updatedAt FROM notes WHERE title LIKE ? OR content LIKE ? ORDER BY updated_at DESC"
      )
      .all(`%${q}%`, `%${q}%`) as Note[];
  }

  addNotebook(name: string, description = ""): Notebook {
    const nb: Notebook = {
      id: randomUUID(),
      name,
      description,
      noteCount: 0,
      createdAt: new Date().toISOString(),
    };
    this.db.prepare("INSERT INTO notebooks VALUES (?,?,?,?,?)").run(nb.id, nb.name, nb.description, nb.noteCount, nb.createdAt);
    return nb;
  }

  listNotebooks(): Notebook[] {
    return this.db
      .prepare("SELECT id, name, description, note_count as noteCount, created_at as createdAt FROM notebooks ORDER BY name ASC")
      .all() as Notebook[];
  }
}
