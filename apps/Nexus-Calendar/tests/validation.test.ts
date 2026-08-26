import { describe, it, expect } from "bun:test";
import { parseEventCreate, parseEventPatch, parseRange, MAX_RANGE_DAYS } from "../src/validation";

const VALID = {
  title: "Standup",
  startTime: "2026-09-01T10:00:00.000Z",
  endTime: "2026-09-01T10:15:00.000Z",
};

function url(qs: string): URL {
  return new URL(`http://127.0.0.1/api/v1/calendar/events${qs}`);
}

describe("parseEventCreate", () => {
  it("accepts a minimal valid event", () => {
    const r = parseEventCreate(VALID);
    expect(r.ok).toBe(true);
    if (r.ok) expect(r.value.title).toBe("Standup");
  });

  it("accepts the optional fields", () => {
    const r = parseEventCreate({ ...VALID, description: "d", location: "l", allDay: true, recurrence: "FREQ=DAILY" });
    expect(r.ok).toBe(true);
    if (r.ok) expect(r.value.allDay).toBe(true);
  });

  it.each([
    ["not an object", "nope"],
    ["null", null],
    ["an array", []],
  ])("rejects %s", (_label, value) => {
    expect(parseEventCreate(value).ok).toBe(false);
  });

  it.each(["title", "startTime", "endTime"])("rejects a missing %s", (field) => {
    const body: Record<string, unknown> = { ...VALID };
    delete body[field];
    expect(parseEventCreate(body).ok).toBe(false);
  });

  it("rejects a blank title", () => {
    expect(parseEventCreate({ ...VALID, title: "   " }).ok).toBe(false);
  });

  it("rejects an oversized title", () => {
    expect(parseEventCreate({ ...VALID, title: "x".repeat(5000) }).ok).toBe(false);
  });

  it("rejects unknown fields rather than ignoring them", () => {
    // ownerSubject is the one that matters: silently dropping it would be safe
    // today and a hole the first time someone loops over the input.
    const r = parseEventCreate({ ...VALID, ownerSubject: "usr-victim" });
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.error).toContain("ownerSubject");
  });

  it.each(["", "yesterday", "2026-13-45T99:99", "2026-09-01T10:00:00+99:00"])(
    "rejects the malformed timestamp %p",
    (ts) => {
      expect(parseEventCreate({ ...VALID, startTime: ts }).ok).toBe(false);
    },
  );

  it("rejects an end at or before the start", () => {
    expect(parseEventCreate({ ...VALID, endTime: VALID.startTime }).ok).toBe(false);
    expect(parseEventCreate({ ...VALID, endTime: "2026-08-01T00:00:00.000Z" }).ok).toBe(false);
  });

  it("rejects wrong types on optional fields", () => {
    expect(parseEventCreate({ ...VALID, allDay: "yes" }).ok).toBe(false);
    expect(parseEventCreate({ ...VALID, description: 42 }).ok).toBe(false);
  });
});

describe("parseEventPatch", () => {
  it("accepts a single field", () => {
    const r = parseEventPatch({ title: "Renamed" });
    expect(r.ok).toBe(true);
    if (r.ok) expect(r.value).toEqual({ title: "Renamed" });
  });

  it("rejects an empty patch", () => {
    expect(parseEventPatch({}).ok).toBe(false);
  });

  it("rejects unknown fields", () => {
    expect(parseEventPatch({ ownerSubject: "usr-victim" }).ok).toBe(false);
  });

  it("rejects a blank title", () => {
    expect(parseEventPatch({ title: "  " }).ok).toBe(false);
  });

  it("rejects an end at or before the start when both are supplied", () => {
    expect(parseEventPatch({ startTime: "2026-09-01T10:00:00.000Z", endTime: "2026-09-01T09:00:00.000Z" }).ok).toBe(false);
  });

  it("allows moving just the end time", () => {
    expect(parseEventPatch({ endTime: "2026-09-01T12:00:00.000Z" }).ok).toBe(true);
  });
});

describe("parseRange", () => {
  it("defaults to a bounded window when nothing is supplied", () => {
    const r = parseRange(url(""));
    expect(r.ok).toBe(true);
    if (r.ok) {
      expect(r.value.from).toMatch(/^\d{4}-\d{2}-\d{2}/);
      expect(r.value.to).toMatch(/^\d{4}-\d{2}-\d{2}/);
    }
  });

  it("accepts explicit date-only bounds", () => {
    const r = parseRange(url("?from=2026-09-01&to=2026-09-30"));
    expect(r.ok).toBe(true);
    if (r.ok) expect(r.value).toEqual({ from: "2026-09-01", to: "2026-09-30" });
  });

  it("rejects a malformed bound", () => {
    expect(parseRange(url("?from=nonsense&to=2026-09-30")).ok).toBe(false);
  });

  it("rejects an inverted window", () => {
    expect(parseRange(url("?from=2026-09-30&to=2026-09-01")).ok).toBe(false);
  });

  it("rejects a window wider than the cap", () => {
    // Unbounded ranges are how one request asks for the entire calendar.
    expect(parseRange(url(`?from=2000-01-01&to=2030-01-01`)).ok).toBe(false);
    expect(MAX_RANGE_DAYS).toBeGreaterThan(31);
  });

  it("accepts a window exactly at the cap", () => {
    const from = new Date("2026-01-01T00:00:00.000Z");
    const to = new Date(from);
    to.setUTCDate(to.getUTCDate() + MAX_RANGE_DAYS);
    const r = parseRange(url(`?from=2026-01-01&to=${to.toISOString().slice(0, 10)}`));
    expect(r.ok).toBe(true);
  });
});
