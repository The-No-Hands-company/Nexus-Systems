import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

/**
 * Schema version, tracked in `PRAGMA user_version`.
 *
 *  0 — the original schema: an `events` table with no owner. Every event was
 *      visible to, and editable by, everyone who could reach the service.
 *  1 — `owner_subject NOT NULL`. Events are private to their owner.
 */
const SCHEMA_VERSION = 1;

export type EventAccess = "owner" | "editor" | "viewer";

export interface CalEvent {
  id: string;
  /** The trusted subject this event belongs to. Immutable after creation. */
  ownerSubject: string;
  title: string;
  description: string | undefined;
  location: string | undefined;
  startTime: string;
  endTime: string;
  allDay: boolean;
  recurrence: string | undefined;
  createdAt: string;
  /**
   * What the requesting caller may do with it. Only "owner" is reachable
   * today; the field exists so viewer/editor sharing slots in without changing
   * the shape callers already read.
   */
  access: EventAccess;
}

export interface EventCreate {
  title: string;
  description?: string | undefined;
  location?: string | undefined;
  startTime: string;
  endTime: string;
  allDay?: boolean | undefined;
  recurrence?: string | undefined;
}

export type EventPatch = Partial<EventCreate>;

/** A half-open window: events overlapping [from, to) are in range. */
export interface EventRange {
  from: string;
  to: string;
}

interface EventRow {
  id: string;
  owner_subject: string;
  title: string;
  description: string | null;
  location: string | null;
  start_time: string;
  end_time: string;
  all_day: number;
  recurrence: string | null;
  created_at: string;
}

function rowToEvent(row: EventRow, access: EventAccess = "owner"): CalEvent {
  return {
    id: row.id,
    ownerSubject: row.owner_subject,
    title: row.title,
    description: row.description ?? undefined,
    location: row.location ?? undefined,
    startTime: row.start_time,
    endTime: row.end_time,
    allDay: row.all_day === 1,
    recurrence: row.recurrence ?? undefined,
    createdAt: row.created_at,
    access,
  };
}

const DATE_ONLY = /^\d{4}-\d{2}-\d{2}$/;

/**
 * Turns a range into an explicit half-open pair of instants.
 *
 * A date-only bound means the whole of that day, which is what someone asking
 * for "1st to the 24th" means — so `to` becomes the start of the 25th, not the
 * start of the 24th. Getting this wrong is how the previous implementation
 * silently dropped every event on the last day of the window.
 */
export function rangeBounds(range: EventRange): { start: string; endExclusive: string } {
  const start = DATE_ONLY.test(range.from) ? `${range.from}T00:00:00.000Z` : range.from;

  let endExclusive = range.to;
  if (DATE_ONLY.test(range.to)) {
    const next = new Date(`${range.to}T00:00:00.000Z`);
    next.setUTCDate(next.getUTCDate() + 1);
    endExclusive = next.toISOString();
  }
  return { start, endExclusive };
}

export class CalendarEngine {
  db: Database;

  /**
   * @param path        SQLite file, or ":memory:".
   * @param legacyOwnerSubject
   *   Required only when migrating a version-0 database that already holds
   *   events. Those rows have no recorded owner and none can be inferred, so
   *   the constructor refuses to continue rather than guess — guessing wrong
   *   hands one person's calendar to another. Never used for new rows.
   */
  constructor(path = ":memory:", options: { legacyOwnerSubject?: string } = {}) {
    this.db = new Database(path);
    this.migrate(options.legacyOwnerSubject);
  }

  private migrate(legacyOwnerSubject?: string): void {
    const current = (this.db.prepare("PRAGMA user_version").get() as { user_version: number }).user_version;
    if (current >= SCHEMA_VERSION) return;

    const legacyRows = this.countLegacyRows();
    if (legacyRows > 0 && !legacyOwnerSubject) {
      // Fail closed. The alternatives are all worse: a NULL owner matches
      // nobody and loses the data, an empty-string owner matches whatever
      // caller happens to send one, and picking "the first user" is a guess.
      throw new Error(
        `legacy_owner_required: ${legacyRows} event(s) predate ownership and have no recorded owner. ` +
        `Set NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT to the subject that should own them.`,
      );
    }

    // One transaction: either the table is rebuilt, backfilled and versioned,
    // or the database is untouched. A half-migrated calendar has no owner
    // column on some rows and is unrecoverable without a backup.
    this.db.exec("BEGIN IMMEDIATE");
    try {
      this.db.exec(`CREATE TABLE IF NOT EXISTS events_v1 (
        id TEXT PRIMARY KEY,
        owner_subject TEXT NOT NULL,
        title TEXT NOT NULL,
        description TEXT,
        location TEXT,
        start_time TEXT NOT NULL,
        end_time TEXT NOT NULL,
        all_day INTEGER NOT NULL DEFAULT 0,
        recurrence TEXT,
        created_at TEXT NOT NULL DEFAULT (datetime('now'))
      )`);

      if (legacyRows > 0) {
        this.db.prepare(
          `INSERT INTO events_v1 (id, owner_subject, title, description, location, start_time, end_time, all_day, recurrence, created_at)
           SELECT id, ?, title, description, location, start_time, end_time,
                  COALESCE(all_day, 0), recurrence, COALESCE(created_at, datetime('now'))
           FROM events`,
        ).run(legacyOwnerSubject!);
      }

      if (this.tableExists("events")) this.db.exec("DROP TABLE events");
      this.db.exec("ALTER TABLE events_v1 RENAME TO events");

      // Every read is scoped by owner and bounded by time; this is that query.
      this.db.exec(
        "CREATE INDEX IF NOT EXISTS idx_events_owner_span ON events (owner_subject, start_time, end_time)",
      );
      this.db.exec(`PRAGMA user_version = ${SCHEMA_VERSION}`);
      this.db.exec("COMMIT");
    } catch (err) {
      this.db.exec("ROLLBACK");
      throw err;
    }
  }

  private tableExists(name: string): boolean {
    const row = this.db
      .prepare("SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?")
      .get(name);
    return row !== null && row !== undefined;
  }

  /** Rows in a pre-ownership `events` table. Zero for a fresh database. */
  private countLegacyRows(): number {
    if (!this.tableExists("events")) return 0;
    const columns = this.db.prepare("PRAGMA table_info(events)").all() as { name: string }[];
    // Already owned — an interrupted run, or a version pragma that was lost.
    if (columns.some((c) => c.name === "owner_subject")) return 0;
    const row = this.db.prepare("SELECT COUNT(*) AS n FROM events").get() as { n: number };
    return row.n;
  }

  /**
   * The owner comes from `ownerSubject`, which callers must take from a trusted
   * identity — never from the request body. Anything owner-shaped inside
   * `input` is ignored by construction: the INSERT below names its columns and
   * reads only known fields.
   */
  createEvent(ownerSubject: string, input: EventCreate): CalEvent {
    const ev: CalEvent = {
      id: randomUUID(),
      ownerSubject,
      title: input.title,
      description: input.description || undefined,
      location: input.location || undefined,
      startTime: input.startTime,
      endTime: input.endTime,
      allDay: input.allDay || false,
      recurrence: input.recurrence || undefined,
      createdAt: new Date().toISOString(),
      access: "owner",
    };
    this.db.prepare(
      `INSERT INTO events (id, owner_subject, title, description, location, start_time, end_time, all_day, recurrence, created_at)
       VALUES (?,?,?,?,?,?,?,?,?,?)`,
    ).run(
      ev.id, ev.ownerSubject, ev.title, ev.description ?? null, ev.location ?? null,
      ev.startTime, ev.endTime, ev.allDay ? 1 : 0, ev.recurrence ?? null, ev.createdAt,
    );
    return ev;
  }

  /**
   * Undefined both when the event does not exist and when it belongs to
   * somebody else — the caller turns that into a 404 either way, so the API
   * cannot be used to probe which event ids are real.
   */
  getEvent(callerSubject: string, id: string): CalEvent | undefined {
    const row = this.db
      .prepare("SELECT * FROM events WHERE id = ? AND owner_subject = ?")
      .get(id, callerSubject) as EventRow | null;
    return row ? rowToEvent(row) : undefined;
  }

  /**
   * Overlap, not containment.
   *
   * An event is in range when it starts before the window ends and ends after
   * the window begins. The previous query asked for events *inside* the window
   * (`start >= from AND end <= to`), which dropped every multi-day event and
   * everything on the final day.
   *
   * datetime() on both sides so a stored "2026-09-01T10:00" and a bound of
   * "2026-09-01T00:00:00.000Z" compare as instants rather than as strings of
   * different lengths, where the shorter one sorts first regardless of when it
   * actually is.
   */
  listEvents(callerSubject: string, range: EventRange): CalEvent[] {
    const { start, endExclusive } = rangeBounds(range);
    const rows = this.db.prepare(
      `SELECT * FROM events
        WHERE owner_subject = ?
          AND datetime(start_time) < datetime(?)
          AND datetime(end_time)   > datetime(?)
        ORDER BY datetime(start_time), id`,
    ).all(callerSubject, endExclusive, start) as EventRow[];
    return rows.map((r) => rowToEvent(r));
  }

  /**
   * Only the owner's own row is touched, and `owner_subject` is not in the SET
   * list — an event cannot be handed to another account through a patch.
   */
  updateEvent(callerSubject: string, id: string, patch: EventPatch): CalEvent | undefined {
    const existing = this.getEvent(callerSubject, id);
    if (!existing) return undefined;

    const merged = {
      title: patch.title ?? existing.title,
      description: patch.description ?? existing.description,
      location: patch.location ?? existing.location,
      startTime: patch.startTime ?? existing.startTime,
      endTime: patch.endTime ?? existing.endTime,
      allDay: patch.allDay ?? existing.allDay,
      recurrence: patch.recurrence ?? existing.recurrence,
    };

    this.db.prepare(
      `UPDATE events
          SET title=?, description=?, location=?, start_time=?, end_time=?, all_day=?, recurrence=?
        WHERE id=? AND owner_subject=?`,
    ).run(
      merged.title, merged.description ?? null, merged.location ?? null,
      merged.startTime, merged.endTime, merged.allDay ? 1 : 0, merged.recurrence ?? null,
      id, callerSubject,
    );
    return this.getEvent(callerSubject, id);
  }

  deleteEvent(callerSubject: string, id: string): boolean {
    return this.db
      .prepare("DELETE FROM events WHERE id = ? AND owner_subject = ?")
      .run(id, callerSubject).changes > 0;
  }

  close(): void {
    this.db.close();
  }
}
