import { describe, it, expect, afterEach } from "bun:test";
import { Database } from "bun:sqlite";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { CalendarEngine } from "../src/calendar-engine";

const ALICE = "usr-alice";
const BOB = "usr-bob";

const dirs: string[] = [];
function tmpDb(): string {
  const dir = mkdtempSync(join(tmpdir(), "nexus-calendar-test-"));
  dirs.push(dir);
  return join(dir, "calendar.sqlite");
}
afterEach(() => {
  while (dirs.length) rmSync(dirs.pop()!, { recursive: true, force: true });
});

/** The schema as it shipped: no owner column at all. */
function seedLegacyDb(path: string, rows: number): void {
  const db = new Database(path);
  db.exec(`CREATE TABLE IF NOT EXISTS events (
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
  for (let i = 0; i < rows; i++) {
    db.prepare("INSERT INTO events VALUES (?,?,?,?,?,?,?,?,?)").run(
      `legacy-${i}`, `Legacy ${i}`, null, null,
      "2026-09-01T10:00:00.000Z", "2026-09-01T11:00:00.000Z", 0, null,
      "2026-08-01T00:00:00.000Z",
    );
  }
  db.close();
}

describe("migration to owned events", () => {
  it("needs no legacy owner for a fresh database", () => {
    const engine = new CalendarEngine(tmpDb());
    expect(engine.listEvents(ALICE, { from: "2026-01-01", to: "2027-01-01" })).toEqual([]);
    engine.close();
  });

  it("refuses to start when legacy rows exist and no owner is configured", () => {
    // Existing events have no defensible inferred owner. Failing closed is the
    // only safe answer — the alternative is guessing, and a wrong guess hands
    // one user's calendar to another.
    const path = tmpDb();
    seedLegacyDb(path, 3);
    expect(() => new CalendarEngine(path)).toThrow(/legacy_owner_required/);
  });

  it("never makes legacy rows globally visible as a migration shortcut", () => {
    const path = tmpDb();
    seedLegacyDb(path, 2);
    // Must not silently succeed with a null/empty owner that matches everyone.
    let threw = false;
    try { new CalendarEngine(path); } catch { threw = true; }
    expect(threw).toBe(true);
  });

  it("backfills every legacy row when a legacy owner is configured", () => {
    const path = tmpDb();
    seedLegacyDb(path, 3);
    const engine = new CalendarEngine(path, { legacyOwnerSubject: ALICE });
    const mine = engine.listEvents(ALICE, { from: "2026-08-01", to: "2026-10-01" });
    expect(mine).toHaveLength(3);
    expect(mine.every((e) => e.ownerSubject === ALICE)).toBe(true);
    // and they belong to nobody else
    expect(engine.listEvents(BOB, { from: "2026-08-01", to: "2026-10-01" })).toEqual([]);
    engine.close();
  });

  it("is idempotent — reopening an already-migrated database is a no-op", () => {
    const path = tmpDb();
    seedLegacyDb(path, 2);
    new CalendarEngine(path, { legacyOwnerSubject: ALICE }).close();
    // No legacy owner needed the second time: the schema is already current.
    const again = new CalendarEngine(path);
    expect(again.listEvents(ALICE, { from: "2026-08-01", to: "2026-10-01" })).toHaveLength(2);
    again.close();
  });

  it("closes its SQLite handle", () => {
    const engine = new CalendarEngine(tmpDb());
    engine.close();
    expect(() => engine.listEvents(ALICE, { from: "2026-01-01", to: "2027-01-01" })).toThrow();
  });
});

describe("ownership is enforced in the engine, not above it", () => {
  function seeded() {
    const engine = new CalendarEngine(":memory:");
    const mine = engine.createEvent(ALICE, {
      title: "Alice's private thing",
      startTime: "2026-09-10T10:00:00.000Z",
      endTime: "2026-09-10T11:00:00.000Z",
    });
    return { engine, mine };
  }

  it("stamps the owner from the caller, never from input", () => {
    const engine = new CalendarEngine(":memory:");
    const ev = engine.createEvent(ALICE, {
      title: "x",
      startTime: "2026-09-10T10:00:00.000Z",
      endTime: "2026-09-10T11:00:00.000Z",
      // A client trying to plant an event in someone else's calendar.
      ownerSubject: BOB,
    } as never);
    expect(ev.ownerSubject).toBe(ALICE);
    engine.close();
  });

  it("hides another user's event from get", () => {
    const { engine, mine } = seeded();
    expect(engine.getEvent(ALICE, mine.id)).toBeDefined();
    expect(engine.getEvent(BOB, mine.id)).toBeUndefined();
    engine.close();
  });

  it("hides another user's event from list", () => {
    const { engine } = seeded();
    expect(engine.listEvents(ALICE, { from: "2026-09-01", to: "2026-10-01" })).toHaveLength(1);
    expect(engine.listEvents(BOB, { from: "2026-09-01", to: "2026-10-01" })).toHaveLength(0);
    engine.close();
  });

  it("refuses another user's update", () => {
    const { engine, mine } = seeded();
    expect(engine.updateEvent(BOB, mine.id, { title: "hijacked" })).toBeUndefined();
    expect(engine.getEvent(ALICE, mine.id)!.title).toBe("Alice's private thing");
    engine.close();
  });

  it("refuses another user's delete", () => {
    const { engine, mine } = seeded();
    expect(engine.deleteEvent(BOB, mine.id)).toBe(false);
    expect(engine.getEvent(ALICE, mine.id)).toBeDefined();
    engine.close();
  });

  it("does not let an update move an event to another owner", () => {
    const { engine, mine } = seeded();
    engine.updateEvent(ALICE, mine.id, { ownerSubject: BOB } as never);
    expect(engine.getEvent(ALICE, mine.id)!.ownerSubject).toBe(ALICE);
    expect(engine.getEvent(BOB, mine.id)).toBeUndefined();
    engine.close();
  });
});

describe("range queries use interval overlap, not containment", () => {
  // The shipped query was `start_time >= from AND end_time <= to`, which asks
  // for events *contained by* the window. Of the four below it returned one.
  function seeded() {
    const engine = new CalendarEngine(":memory:");
    const add = (title: string, startTime: string, endTime: string) =>
      engine.createEvent(ALICE, { title, startTime, endTime });
    add("inside-the-window",  "2026-09-10T12:00:00.000Z", "2026-09-10T13:00:00.000Z");
    add("last-day-of-range",  "2026-09-24T12:00:00.000Z", "2026-09-24T13:00:00.000Z");
    add("started-before",     "2026-08-24T12:00:00.000Z", "2026-09-02T13:00:00.000Z");
    add("two-week-holiday",   "2026-08-20T00:00:00.000Z", "2026-09-03T00:00:00.000Z");
    add("entirely-after",     "2026-11-01T00:00:00.000Z", "2026-11-02T00:00:00.000Z");
    add("entirely-before",    "2026-07-01T00:00:00.000Z", "2026-07-02T00:00:00.000Z");
    return engine;
  }

  it("returns every event overlapping the window", () => {
    const engine = seeded();
    const titles = engine
      .listEvents(ALICE, { from: "2026-08-25", to: "2026-09-24" })
      .map((e) => e.title);
    expect(titles).toContain("inside-the-window");
    expect(titles).toContain("last-day-of-range");   // was dropped: "…-09-24T12:00" > "…-09-24"
    expect(titles).toContain("started-before");      // was dropped: began before the window
    expect(titles).toContain("two-week-holiday");    // was dropped: began before the window
    engine.close();
  });

  it("excludes events that do not overlap at all", () => {
    const engine = seeded();
    const titles = engine
      .listEvents(ALICE, { from: "2026-08-25", to: "2026-09-24" })
      .map((e) => e.title);
    expect(titles).not.toContain("entirely-after");
    expect(titles).not.toContain("entirely-before");
    engine.close();
  });

  it("treats an event touching the boundary as outside it", () => {
    // Half-open interval [start, end): an event ending exactly when the window
    // starts does not overlap it, and neither does one starting exactly when it
    // ends. Stated with explicit instants — a date-only bound means the whole
    // of that day, which is a different question (see the test below).
    const engine = new CalendarEngine(":memory:");
    engine.createEvent(ALICE, { title: "ends-at-start", startTime: "2026-09-01T00:00:00.000Z", endTime: "2026-09-10T00:00:00.000Z" });
    engine.createEvent(ALICE, { title: "starts-at-end", startTime: "2026-09-20T00:00:00.000Z", endTime: "2026-09-25T00:00:00.000Z" });
    engine.createEvent(ALICE, { title: "overlaps-by-a-minute", startTime: "2026-09-19T23:59:00.000Z", endTime: "2026-09-25T00:00:00.000Z" });
    const titles = engine
      .listEvents(ALICE, { from: "2026-09-10T00:00:00.000Z", to: "2026-09-20T00:00:00.000Z" })
      .map((e) => e.title);
    expect(titles).not.toContain("ends-at-start");
    expect(titles).not.toContain("starts-at-end");
    expect(titles).toContain("overlaps-by-a-minute");
    engine.close();
  });

  it("reads a date-only bound as the whole of that day", () => {
    // "to=2026-09-20" means "through the end of the 20th", which is what a
    // person asking for a date range means. The window end therefore lands at
    // the start of the 21st, so anything on the 20th is included.
    const engine = new CalendarEngine(":memory:");
    engine.createEvent(ALICE, { title: "late-on-the-last-day", startTime: "2026-09-20T23:30:00.000Z", endTime: "2026-09-20T23:45:00.000Z" });
    engine.createEvent(ALICE, { title: "next-morning", startTime: "2026-09-21T09:00:00.000Z", endTime: "2026-09-21T10:00:00.000Z" });
    const titles = engine.listEvents(ALICE, { from: "2026-09-10", to: "2026-09-20" }).map((e) => e.title);
    expect(titles).toContain("late-on-the-last-day");
    expect(titles).not.toContain("next-morning");
    engine.close();
  });

  it("orders results by start time", () => {
    const engine = seeded();
    const starts = engine.listEvents(ALICE, { from: "2026-01-01", to: "2027-01-01" }).map((e) => e.startTime);
    expect([...starts].sort()).toEqual(starts);
    engine.close();
  });
});
