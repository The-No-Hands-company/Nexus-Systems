import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface CalEvent {
  id: string;
  title: string;
  description: string | undefined;
  location: string | undefined;
  startTime: string;
  endTime: string;
  allDay: boolean;
  recurrence: string | undefined;
  createdAt: string;
}

function rowToEvent(row: any): CalEvent {
    return {
      id: row.id,
      title: row.title,
      description: row.description ?? undefined,
      location: row.location ?? undefined,
      startTime: row.start_time,
      endTime: row.end_time,
      allDay: row.all_day === 1,
      recurrence: row.recurrence ?? undefined,
      createdAt: row.created_at,
    };
  }

export class CalendarEngine {
  db: Database;
  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(`CREATE TABLE IF NOT EXISTS events (
      id TEXT PRIMARY KEY,
      title TEXT NOT NULL,
      description TEXT,
      location TEXT,
      start_time TEXT NOT NULL,
      end_time TEXT NOT NULL,
      all_day INTEGER DEFAULT 0,
      recurrence TEXT,
      created_at TEXT DEFAULT (datetime('now'))
    )`);
  }

  createEvent(e: { title: string; description?: string; location?: string; startTime: string; endTime: string; allDay?: boolean; recurrence?: string }): CalEvent {
    const ev: CalEvent = {
      id: randomUUID(),
      title: e.title,
      description: e.description || undefined,
      location: e.location || undefined,
      startTime: e.startTime,
      endTime: e.endTime,
      allDay: e.allDay || false,
      recurrence: e.recurrence || undefined,
      createdAt: new Date().toISOString(),
    };
    this.db.prepare("INSERT INTO events VALUES (?,?,?,?,?,?,?,?,?)").run(
      ev.id, ev.title, ev.description ?? null, ev.location ?? null,
      ev.startTime, ev.endTime, ev.allDay ? 1 : 0, ev.recurrence ?? null, ev.createdAt,
    );
    return ev;
  }

  getEvent(id: string): CalEvent | undefined {
    const row = this.db.prepare("SELECT * FROM events WHERE id = ?").get(id); return row ? rowToEvent(row) : undefined;
  }

  listEvents(from: string, to: string): CalEvent[] {
    return (this.db.prepare("SELECT * FROM events WHERE start_time >= ? AND end_time <= ? ORDER BY start_time").all(from, to) as any[]).map(rowToEvent);
  }

  updateEvent(id: string, patch: Partial<CalEvent>): CalEvent | undefined {
    const existing = this.getEvent(id);
    if (!existing) return undefined;
    const merged = { ...existing, ...patch };
    this.db.prepare(
      "UPDATE events SET title=?, description=?, location=?, start_time=?, end_time=?, all_day=?, recurrence=? WHERE id=?",
    ).run(merged.title, merged.description ?? null, merged.location ?? null,
      merged.startTime, merged.endTime, merged.allDay ? 1 : 0, merged.recurrence ?? null, id);
    return this.getEvent(id);
  }

  deleteEvent(id: string): boolean {
    return this.db.prepare("DELETE FROM events WHERE id = ?").run(id).changes > 0;
  }
}
