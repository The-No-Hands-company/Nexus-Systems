import { Database } from "bun:sqlite";
import { createHash, randomBytes, randomUUID } from "node:crypto";
import { canonicalTimestamp } from "./validation";

export type EventAccess = "owner" | "editor" | "viewer";
export type EventPermission = "viewer" | "editor";

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

export interface EventShare {
  eventId: string;
  subject: string;
  permission: EventPermission;
  createdBy: string;
  createdAt: string;
  updatedAt: string;
}

export interface PublicEvent {
  title: string;
  startTime: string;
  endTime: string;
  allDay: boolean;
  location: string | null;
  description: string | null;
}

export interface PublicShare {
  token: string;
  publicPath: string;
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
  access?: EventAccess;
};

type EventShareRow = {
  event_id: string;
  grantee_subject: string;
  permission: EventPermission;
  created_by: string;
  created_at: string;
  updated_at: string;
};

type PublicEventRow = {
  title: string;
  start_time: string;
  end_time: string;
  all_day: number;
  location: string | null;
  description: string | null;
};

const PUBLIC_TOKEN = /^[A-Za-z0-9_-]{43}$/;

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
    access: row.access ?? "owner",
  };
}

function rowToShare(row: EventShareRow): EventShare {
  return {
    eventId: row.event_id,
    subject: row.grantee_subject,
    permission: row.permission,
    createdBy: row.created_by,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
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

function createEventSharesTable(db: Database): void {
  db.exec(`CREATE TABLE IF NOT EXISTS event_shares (
    event_id TEXT NOT NULL REFERENCES events(id) ON DELETE CASCADE,
    grantee_subject TEXT NOT NULL,
    permission TEXT NOT NULL CHECK (permission IN ('viewer', 'editor')),
    created_by TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    PRIMARY KEY (event_id, grantee_subject)
  )`);
}

function createPublicEventSharesTable(db: Database): void {
  db.exec(`CREATE TABLE IF NOT EXISTS public_event_shares (
    event_id TEXT PRIMARY KEY REFERENCES events(id) ON DELETE CASCADE,
    token_digest TEXT NOT NULL UNIQUE,
    created_at TEXT NOT NULL
  )`);
}

function tokenDigest(token: string): string {
  return createHash("sha256").update(token).digest("hex");
}

export class CalendarEngine {
  db: Database;

  constructor(path = ":memory:", options: { legacyOwnerSubject?: string } = {}) {
    this.db = new Database(path);
    this.db.exec("PRAGMA foreign_keys = ON");

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
    createEventSharesTable(this.db);
    createPublicEventSharesTable(this.db);
    this.db.exec("CREATE INDEX IF NOT EXISTS events_owner_start_end_idx ON events (owner_subject, start_time, end_time)");
    this.db.exec("CREATE INDEX IF NOT EXISTS event_shares_grantee_event_idx ON event_shares (grantee_subject, event_id)");
    if (currentVersion < 2) this.db.exec("PRAGMA user_version = 2");
    this.db.exec("COMMIT");
  }

  private transaction<T>(operation: () => T): T {
    this.db.exec("BEGIN IMMEDIATE");
    try {
      const result = operation();
      this.db.exec("COMMIT");
      return result;
    } catch (error) {
      this.db.exec("ROLLBACK");
      throw error;
    }
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
    const row = this.db.prepare(`SELECT events.*, CASE
      WHEN events.owner_subject = ? THEN 'owner'
      ELSE event_shares.permission
    END AS access
    FROM events
    LEFT JOIN event_shares ON event_shares.event_id = events.id AND event_shares.grantee_subject = ?
    WHERE events.id = ? AND (events.owner_subject = ? OR event_shares.grantee_subject IS NOT NULL)`)
      .get(callerSubject, callerSubject, id, callerSubject) as EventRow | null;
    return row ? rowToEvent(row) : undefined;
  }

  listEvents(callerSubject: string, range: EventRange): CalEvent[] {
    return (this.db.prepare(`SELECT events.*, CASE
      WHEN events.owner_subject = ? THEN 'owner'
      ELSE event_shares.permission
    END AS access
    FROM events
    LEFT JOIN event_shares ON event_shares.event_id = events.id AND event_shares.grantee_subject = ?
    WHERE (events.owner_subject = ? OR event_shares.grantee_subject IS NOT NULL)
      AND events.start_time < ? AND events.end_time > ?
    ORDER BY events.start_time`).all(callerSubject, callerSubject, callerSubject, range.to, range.from) as EventRow[]).map(rowToEvent);
  }

  updateEvent(callerSubject: string, id: string, patch: Partial<EventCreate>): CalEvent | undefined {
    const existing = this.getEvent(callerSubject, id);
    if (!existing || existing.access === "viewer") return undefined;
    const merged = { ...existing, ...patch };
    this.db.prepare(
      `UPDATE events SET title=?, description=?, location=?, start_time=?, end_time=?, all_day=?, recurrence=?
      WHERE id=? AND (owner_subject=? OR EXISTS (
        SELECT 1 FROM event_shares
        WHERE event_shares.event_id = events.id AND event_shares.grantee_subject = ? AND event_shares.permission = 'editor'
      ))`,
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
      callerSubject,
    );
    return this.getEvent(callerSubject, id);
  }

  deleteEvent(callerSubject: string, id: string): boolean {
    return this.db.prepare("DELETE FROM events WHERE id = ? AND owner_subject = ?").run(id, callerSubject).changes > 0;
  }

  listShares(owner: string, eventId: string): EventShare[] | undefined {
    return this.transaction(() => {
      const event = this.db.prepare("SELECT 1 FROM events WHERE id = ? AND owner_subject = ?").get(eventId, owner);
      if (!event) return undefined;
      return (this.db.prepare(`SELECT event_id, grantee_subject, permission, created_by, created_at, updated_at
        FROM event_shares WHERE event_id = ? ORDER BY grantee_subject`).all(eventId) as EventShareRow[]).map(rowToShare);
    });
  }

  upsertShare(owner: string, eventId: string, subject: string, permission: EventPermission): EventShare | undefined {
    return this.transaction(() => {
      const normalizedOwner = normalizedOwnerSubject(owner);
      const grantee = normalizedOwnerSubject(subject);
      if (!normalizedOwner || !grantee || normalizedOwner === grantee) return undefined;
      const event = this.db.prepare("SELECT 1 FROM events WHERE id = ? AND owner_subject = ?").get(eventId, normalizedOwner);
      if (!event) return undefined;
      const now = new Date().toISOString();
      this.db.prepare(`INSERT INTO event_shares (
        event_id, grantee_subject, permission, created_by, created_at, updated_at
      ) VALUES (?, ?, ?, ?, ?, ?)
      ON CONFLICT(event_id, grantee_subject) DO UPDATE SET
        permission = excluded.permission,
        created_by = excluded.created_by,
        updated_at = excluded.updated_at`).run(eventId, grantee, permission, normalizedOwner, now, now);
      const row = this.db.prepare(`SELECT event_id, grantee_subject, permission, created_by, created_at, updated_at
        FROM event_shares WHERE event_id = ? AND grantee_subject = ?`).get(eventId, grantee) as EventShareRow;
      return rowToShare(row);
    });
  }

  deleteShare(owner: string, eventId: string, subject: string): boolean {
    return this.transaction(() => this.db.prepare(`DELETE FROM event_shares
      WHERE event_id = ? AND grantee_subject = ? AND EXISTS (
        SELECT 1 FROM events WHERE events.id = event_shares.event_id AND events.owner_subject = ?
      )`).run(eventId, subject, owner).changes > 0);
  }

  createPublicShare(owner: string, eventId: string): PublicShare | undefined {
    return this.transaction(() => {
      const normalizedOwner = normalizedOwnerSubject(owner);
      if (!normalizedOwner) return undefined;
      const event = this.db.prepare("SELECT 1 FROM events WHERE id = ? AND owner_subject = ?").get(eventId, normalizedOwner);
      if (!event) return undefined;

      const token = randomBytes(32).toString("base64url");
      this.db.prepare(`INSERT INTO public_event_shares (event_id, token_digest, created_at)
        VALUES (?, ?, ?)
        ON CONFLICT(event_id) DO UPDATE SET token_digest = excluded.token_digest, created_at = excluded.created_at`)
        .run(eventId, tokenDigest(token), new Date().toISOString());
      return { token, publicPath: `/api/v1/calendar/public/${token}` };
    });
  }

  revokePublicShare(owner: string, eventId: string): boolean | undefined {
    return this.transaction(() => {
      const normalizedOwner = normalizedOwnerSubject(owner);
      if (!normalizedOwner) return undefined;
      const event = this.db.prepare("SELECT 1 FROM events WHERE id = ? AND owner_subject = ?").get(eventId, normalizedOwner);
      if (!event) return undefined;
      return this.db.prepare("DELETE FROM public_event_shares WHERE event_id = ?").run(eventId).changes > 0;
    });
  }

  getPublicEvent(token: string): PublicEvent | undefined {
    if (!PUBLIC_TOKEN.test(token)) return undefined;
    const row = this.db.prepare(`SELECT events.title, events.start_time, events.end_time, events.all_day, events.location, events.description
      FROM events INNER JOIN public_event_shares ON public_event_shares.event_id = events.id
      WHERE public_event_shares.token_digest = ?`).get(tokenDigest(token)) as PublicEventRow | null;
    return row ? {
      title: row.title,
      startTime: row.start_time,
      endTime: row.end_time,
      allDay: row.all_day === 1,
      location: row.location,
      description: row.description,
    } : undefined;
  }

  close(): void {
    this.db.close();
  }
}
