import { describe, expect, it } from "bun:test";
import { parseEventCreate, parseEventPatch, parseRange } from "../src/validation";

const event = {
  title: "Planning",
  startTime: "2026-09-01T10:00:00.000Z",
  endTime: "2026-09-01T11:00:00.000Z",
};

describe("Calendar request validation", () => {
  it("rejects blank and oversized titles", () => {
    expect(parseEventCreate({ ...event, title: "   " })).toMatchObject({ ok: false });
    expect(parseEventCreate({ ...event, title: "a".repeat(513) })).toMatchObject({ ok: false });
  });

  it("rejects unknown event fields", () => {
    expect(parseEventCreate({ ...event, ownerSubject: "usr-victim" })).toMatchObject({ ok: false });
    expect(parseEventPatch({ title: "Updated", unexpected: true })).toMatchObject({ ok: false });
  });

  it("rejects malformed and non-increasing event times", () => {
    expect(parseEventCreate({ ...event, startTime: "not-a-date" })).toMatchObject({ ok: false });
    expect(parseEventCreate({ ...event, startTime: "09/01/2026" })).toMatchObject({ ok: false });
    expect(parseEventCreate({ ...event, startTime: "2026-02-29T10:00:00.000Z" })).toMatchObject({ ok: false });
    expect(parseEventCreate({ ...event, endTime: event.startTime })).toMatchObject({ ok: false });
  });

  it("normalizes accepted offset timestamps to UTC ISO strings", () => {
    expect(parseEventCreate({
      ...event,
      startTime: "2026-09-01T10:00:00-05:00",
      endTime: "2026-09-01T11:00:00-05:00",
    })).toEqual({
      ok: true,
      value: {
        ...event,
        startTime: "2026-09-01T15:00:00.000Z",
        endTime: "2026-09-01T16:00:00.000Z",
      },
    });
  });

  it("rejects an empty patch", () => {
    expect(parseEventPatch({})).toMatchObject({ ok: false });
  });

  it("normalizes an allowed event body", () => {
    expect(parseEventCreate({ ...event, title: "  Planning  " })).toEqual({
      ok: true,
      value: { ...event, title: "Planning" },
    });
  });

  it("rejects malformed and oversized date windows", () => {
    expect(parseRange(new URL("http://calendar.test/events?from=nope&to=2026-09-01"))).toMatchObject({ ok: false });
    expect(parseRange(new URL("http://calendar.test/events?from=2026-01-01&to=2027-02-02"))).toMatchObject({ ok: false });
  });
});
