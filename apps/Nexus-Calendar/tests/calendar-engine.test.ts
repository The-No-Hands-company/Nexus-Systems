import { afterEach, describe, expect, it } from "bun:test";
import { Database } from "bun:sqlite";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { CalendarEngine } from "../src/calendar-engine";

const cleanupPaths: string[] = [];

afterEach(async () => {
  await Promise.all(cleanupPaths.splice(0).map((path) => rm(path, { recursive: true, force: true })));
});

async function temporaryDatabasePath(): Promise<string> {
  const directory = await mkdtemp(join(tmpdir(), "nexus-calendar-engine-"));
  cleanupPaths.push(directory);
  return join(directory, "calendar.sqlite");
}

function createLegacyDatabase(path: string, titles: string[]): void {
  const db = new Database(path);
  db.exec(`CREATE TABLE events (
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
  const insert = db.prepare(
    "INSERT INTO events (id, title, start_time, end_time, created_at) VALUES (?, ?, ?, ?, ?)",
  );
  for (const [index, title] of titles.entries()) {
    insert.run(
      `legacy-${index}`,
      title,
      "2026-09-01T10:00:00.000Z",
      "2026-09-01T11:00:00.000Z",
      "2026-08-25T10:00:00.000Z",
    );
  }
  db.close();
}

function createNullableOwnerDatabase(path: string): void {
  const db = new Database(path);
  db.exec(`CREATE TABLE events (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    description TEXT,
    location TEXT,
    start_time TEXT NOT NULL,
    end_time TEXT NOT NULL,
    all_day INTEGER DEFAULT 0,
    recurrence TEXT,
    created_at TEXT DEFAULT (datetime('now')),
    owner_subject TEXT
  )`);
  db.prepare(
    "INSERT INTO events (id, title, start_time, end_time, created_at, owner_subject) VALUES (?, ?, ?, ?, ?, ?)",
  ).run(
    "partially-deployed-unowned",
    "Unowned partial deployment",
    "2026-09-01T10:00:00.000Z",
    "2026-09-01T11:00:00.000Z",
    "2026-08-25T10:00:00.000Z",
    null,
  );
  db.close();
}

function createOwnedOffsetDatabase(path: string): void {
  const db = new Database(path);
  db.exec(`CREATE TABLE events (
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
  db.prepare(`INSERT INTO events (
    id, title, start_time, end_time, created_at, owner_subject
  ) VALUES (?, ?, ?, ?, ?, ?)`).run(
    "offset-before-canonical-migration",
    "Offset legacy event",
    "2026-09-01T10:00:00-05:00",
    "2026-09-01T11:00:00-05:00",
    "2026-08-25T10:00:00.000Z",
    "usr-alice",
  );
  db.exec("PRAGMA user_version = 1");
  db.close();
}

const eventInput = {
  title: "Private planning",
  startTime: "2026-09-01T10:00:00.000Z",
  endTime: "2026-09-01T11:00:00.000Z",
};

const septemberRange = {
  from: "2026-09-01T00:00:00.000Z",
  to: "2026-09-30T23:59:59.999Z",
};

describe("CalendarEngine owned-event migration", () => {
  it("canonicalizes owned pre-existing offset timestamps before overlap queries", async () => {
    const path = await temporaryDatabasePath();
    createOwnedOffsetDatabase(path);

    const engine = new CalendarEngine(path);
    const events = engine.listEvents("usr-alice", {
      from: "2026-09-01T14:30:00.000Z",
      to: "2026-09-01T15:30:00.000Z",
    });

    expect(events).toHaveLength(1);
    expect(events[0]).toMatchObject({
      startTime: "2026-09-01T15:00:00.000Z",
      endTime: "2026-09-01T16:00:00.000Z",
    });
    expect(engine.db.query("PRAGMA user_version").get()).toMatchObject({ user_version: 2 });
    engine.close();
  });

  it("finds an offset event that overlaps a UTC query window", () => {
    const engine = new CalendarEngine();
    engine.createEvent("usr-alice", {
      title: "Offset planning",
      startTime: "2026-09-01T10:00:00-05:00",
      endTime: "2026-09-01T11:00:00-05:00",
    });

    const events = engine.listEvents("usr-alice", {
      from: "2026-09-01T14:30:00.000Z",
      to: "2026-09-01T15:30:00.000Z",
    });

    expect(events).toHaveLength(1);
    expect(events[0]?.startTime).toBe("2026-09-01T15:00:00.000Z");
    expect(events[0]?.endTime).toBe("2026-09-01T16:00:00.000Z");
    engine.close();
  });

  it("fails closed when a legacy database has rows but no configured owner", async () => {
    const path = await temporaryDatabasePath();
    createLegacyDatabase(path, ["Unowned event"]);

    expect(() => new CalendarEngine(path)).toThrow("legacy_owner_required");
  });

  it("fails closed for a nullable owner_subject schema containing an unowned row", async () => {
    const path = await temporaryDatabasePath();
    createNullableOwnerDatabase(path);

    expect(() => new CalendarEngine(path)).toThrow("legacy_owner_required");

    const engine = new CalendarEngine(path, { legacyOwnerSubject: "usr-founder" });
    expect(engine.getEvent("usr-founder", "partially-deployed-unowned")).toMatchObject({
      ownerSubject: "usr-founder",
      access: "owner",
    });
    expect(engine.db.query("PRAGMA table_info(events)").all()).toContainEqual(
      expect.objectContaining({ name: "owner_subject", type: "TEXT", notnull: 1 }),
    );
    engine.close();
  });

  it("backfills every legacy row from NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT", async () => {
    const path = await temporaryDatabasePath();
    createLegacyDatabase(path, ["One", "Two"]);
    const previous = process.env.NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT;
    process.env.NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT = "usr-founder";

    try {
      const engine = new CalendarEngine(path, {
        legacyOwnerSubject: process.env.NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT,
      });
      const events = engine.listEvents("usr-founder", septemberRange);

      expect(events).toHaveLength(2);
      expect(events.map((event) => event.ownerSubject)).toEqual(["usr-founder", "usr-founder"]);
      expect(events.map((event) => event.access)).toEqual(["owner", "owner"]);
      expect(engine.db.query("PRAGMA table_info(events)").all()).toContainEqual(
        expect.objectContaining({ name: "owner_subject", notnull: 1 }),
      );
      expect(engine.db.query("PRAGMA index_list(events)").all()).toContainEqual(
        expect.objectContaining({ name: "events_owner_start_end_idx" }),
      );
      expect(engine.db.query("PRAGMA user_version").get()).toMatchObject({ user_version: 2 });
      engine.close();
    } finally {
      if (previous === undefined) delete process.env.NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT;
      else process.env.NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT = previous;
    }
  });

  it("initializes an empty legacy database without choosing an owner", async () => {
    const path = await temporaryDatabasePath();
    createLegacyDatabase(path, []);

    const engine = new CalendarEngine(path);
    const event = engine.createEvent("usr-alice", eventInput);

    expect(event).toMatchObject({ ownerSubject: "usr-alice", access: "owner" });
    expect(engine.listEvents("usr-alice", septemberRange)).toHaveLength(1);
    expect(engine.listEvents("usr-bob", septemberRange)).toHaveLength(0);
    expect(engine.db.query("PRAGMA user_version").get()).toMatchObject({ user_version: 2 });
    engine.close();
  });

  it("closes its SQLite handle", () => {
    const engine = new CalendarEngine();
    engine.close();

    expect(() => engine.createEvent("usr-alice", eventInput)).toThrow();
  });
});
