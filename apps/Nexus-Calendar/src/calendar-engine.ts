import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { canonicalTimestamp } from "./validation";

export type EventAccess = "owner" | "editor" | "viewer";

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
  ownerSubject: string;
  access: EventAccess;
}

export interface EventCreate {
  title: string;
  description?: string;
  location?: string;
  startTime: string;
  endTime: string;
  allDay?: boolean;
  recurrence?: string;
}

export interface EventRange {
  from: string;
  to: string;
}

type EventRow = {
  id: string;
  title: string;
  description: string | null;
  location: string | null;
  start_time: string;
  end_time: string;
  all_day: number;
  recurrence: string | null;
  created_at: string;
  owner_subject: string;
};

function rowToEvent(row: EventRow): CalEvent {
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
    ownerSubject: row.owner_subject,
    access: "owner",
  };
}

function normalizedOwnerSubject(value: string | undefined): string | undefined {
  const subject = value?.trim();
  return subject === "" || subject === undefined ? undefined : subject;
}

function canonicalUtcTimestamp(value: string): string {
  const canonical = canonicalTimestamp(value);
  if (!canonical) throw new Error("invalid_event_timestamp");
  return canonical;
}

function createOwnedEventsTable(db: Database, name = "events"): void {
  db.exec(`CREATE TABLE ${name} (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    description TEXT,
    location TEXT,
    start_time TEXT NOT NULL,
    end_time TEXT NOT NULL,
    all_day INTEGER DEFAULT 0,
    recurrence TEXT,
    created_at TEXT DEFAULT (datetime('now')),
    owner_subject TEXT NOT NULL
  )`);
}

export class CalendarEngine {
  db: Database;

  constructor(path = ":memory:", options: { legacyOwnerSubject?: string } = {}) {
    this.db = new Database(path);

    try {
      this.migrate(options);
    } catch (error) {
      if (this.db.inTransaction) this.db.exec("ROLLBACK");
      this.db.close();
      throw error;
    }
  }

  private migrate(options: { legacyOwnerSubject?: string }): void {
    this.db.exec("BEGIN IMMEDIATE");

    const currentVersion = (this.db.query("PRAGMA user_version").get() as { user_version: number }).user_version;
    const eventsTable = this.db
      .prepare("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'events'")
      .get();

    if (!eventsTable) {
      createOwnedEventsTable(this.db);
    } else {
      const columns = this.db.query("PRAGMA table_info(events)").all() as {
        name: string;
        type: string;
        notnull: number;
      }[];
      const ownershipColumn = columns.find((column) => column.name === "owner_subject");
      const isOwnedSchema = ownershipColumn?.type.toUpperCase() === "TEXT" && ownershipColumn.notnull === 1;

      if (!isOwnedSchema) {
        const rowCount = (this.db.query("SELECT COUNT(*) AS count FROM events").get() as { count: number }).count;
        const unownedRows = ownershipColumn
          ? (this.db.query("SELECT COUNT(*) AS count FROM events WHERE owner_subject IS NULL").get() as { count: number }).count
          : rowCount;
        const legacyOwner = normalizedOwnerSubject(options.legacyOwnerSubject);
        if (unownedRows > 0 && !legacyOwner) throw new Error("legacy_owner_required");

        createOwnedEventsTable(this.db, "events_owned_migration");
        if (rowCount > 0) {
          this.db.prepare(`INSERT INTO events_owned_migration (
            id, title, description, location, start_time, end_time, all_day, recurrence, created_at, owner_subject
          ) SELECT id, title, description, location, start_time, end_time, all_day, recurrence, created_at,
            ${ownershipColumn ? "COALESCE(owner_subject, ?)" : "?"}
          FROM events`).run(legacyOwner ?? null);
        }
        this.db.exec("DROP TABLE events");
        this.db.exec("ALTER TABLE events_owned_migration RENAME TO events");
      }
    }

    if (currentVersion < 2) this.canonicalizeStoredTimestamps();
    this.db.exec("CREATE INDEX IF NOT EXISTS events_owner_start_end_idx ON events (owner_subject, start_time, end_time)");
    if (currentVersion < 2) this.db.exec("PRAGMA user_version = 2");
    this.db.exec("COMMIT");
  }

  private canonicalizeStoredTimestamps(): void {
    const rows = this.db.prepare("SELECT id, start_time, end_time FROM events").all() as {
      id: string;
      start_time: string;
      end_time: string;
    }[];
    const update = this.db.prepare("UPDATE events SET start_time = ?, end_time = ? WHERE id = ?");
    for (const row of rows) {
      update.run(canonicalUtcTimestamp(row.start_time), canonicalUtcTimestamp(row.end_time), row.id);
    }
  }

  createEvent(subject: string, input: EventCreate): CalEvent {
    const eventOwner = normalizedOwnerSubject(subject);
    if (!eventOwner) throw new Error("owner_subject_required");

    const event: CalEvent = {
      id: randomUUID(),
      title: input.title,
      description: input.description || undefined,
      location: input.location || undefined,
      startTime: canonicalUtcTimestamp(input.startTime),
      endTime: canonicalUtcTimestamp(input.endTime),
      allDay: input.allDay || false,
      recurrence: input.recurrence || undefined,
      createdAt: new Date().toISOString(),
      ownerSubject: eventOwner,
      access: "owner",
    };
    this.db.prepare(`INSERT INTO events (
      id, title, description, location, start_time, end_time, all_day, recurrence, created_at, owner_subject
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`).run(
      event.id,
      event.title,
      event.description ?? null,
      event.location ?? null,
      event.startTime,
      event.endTime,
      event.allDay ? 1 : 0,
      event.recurrence ?? null,
      event.createdAt,
      event.ownerSubject,
    );
    return event;
  }

  getEvent(callerSubject: string, id: string): CalEvent | undefined {
    const row = this.db.prepare("SELECT * FROM events WHERE id = ? AND owner_subject = ?").get(id, callerSubject) as EventRow | null;
    return row ? rowToEvent(row) : undefined;
  }

  listEvents(callerSubject: string, range: EventRange): CalEvent[] {
    return (this.db.prepare(`SELECT * FROM events
      WHERE owner_subject = ? AND start_time < ? AND end_time > ?
      ORDER BY start_time`).all(callerSubject, range.to, range.from) as EventRow[]).map(rowToEvent);
  }

  updateEvent(callerSubject: string, id: string, patch: Partial<EventCreate>): CalEvent | undefined {
    const existing = this.getEvent(callerSubject, id);
    if (!existing) return undefined;
    const merged = { ...existing, ...patch };
    this.db.prepare(
      "UPDATE events SET title=?, description=?, location=?, start_time=?, end_time=?, all_day=?, recurrence=? WHERE id=? AND owner_subject=?",
    ).run(
      merged.title,
      merged.description ?? null,
      merged.location ?? null,
      canonicalUtcTimestamp(merged.startTime),
      canonicalUtcTimestamp(merged.endTime),
      merged.allDay ? 1 : 0,
      merged.recurrence ?? null,
      id,
      callerSubject,
    );
    return this.getEvent(callerSubject, id);
  }

  deleteEvent(callerSubject: string, id: string): boolean {
    return this.db.prepare("DELETE FROM events WHERE id = ? AND owner_subject = ?").run(id, callerSubject).changes > 0;
  }

  close(): void {
    this.db.close();
  }
}
