import { describe, expect, it } from "bun:test";
import { WorkingCalendar, fromDay, toDay, weekday } from "../src/schedule/calendar";
import type { CalendarSpec } from "../src/schedule/types";
import { mulberry32, randomInt } from "./support/random";

const WEEKDAYS: CalendarSpec = { workingWeekdays: [1, 2, 3, 4, 5], exceptions: [] };

describe("day numbers", () => {
  it("anchors day 0 at 1970-01-01, a Thursday", () => {
    expect(toDay("1970-01-01")).toBe(0);
    expect(weekday(0)).toBe(4);
    expect(weekday(toDay("2026-09-07"))).toBe(1); // a Monday
  });

  it("round-trips every day across leap years", () => {
    for (let day = toDay("2023-12-25"); day < toDay("2025-01-10"); day++) {
      expect(toDay(fromDay(day))).toBe(day);
    }
  });

  it("rejects strings that are not real dates", () => {
    for (const bad of [
      "2026-02-30",
      "2026-13-01",
      "2026-9-7",
      "20260907",
      "",
      "2026-09-07T00:00",
    ]) {
      expect(() => toDay(bad)).toThrow(RangeError);
    }
  });
});

describe("WorkingCalendar", () => {
  const cal = new WorkingCalendar(WEEKDAYS);
  const monday = toDay("2026-09-07");

  it("counts only working days", () => {
    expect(cal.indexOf(monday + 1) - cal.indexOf(monday)).toBe(1); // Mon is working
    expect(cal.indexOf(monday + 7) - cal.indexOf(monday)).toBe(5); // one week
  });

  it("gives a weekend day the index of the following Monday", () => {
    expect(cal.indexOf(monday - 1)).toBe(cal.indexOf(monday)); // Sunday
    expect(cal.indexOf(monday - 2)).toBe(cal.indexOf(monday)); // Saturday
  });

  it("maps an index back to its working day", () => {
    expect(fromDay(cal.dayAt(cal.indexOf(monday) + 4))).toBe("2026-09-11"); // Friday
    expect(fromDay(cal.dayAt(cal.indexOf(monday) + 5))).toBe("2026-09-14"); // next Monday
  });

  it("applies holidays and working exceptions", () => {
    const holidays = new WorkingCalendar({
      workingWeekdays: [1, 2, 3, 4, 5],
      exceptions: [
        { date: "2026-09-08", working: false }, // a Tuesday off
        { date: "2026-09-12", working: true }, // a working Saturday
      ],
    });
    expect(holidays.isWorking(toDay("2026-09-08"))).toBe(false);
    expect(holidays.isWorking(toDay("2026-09-12"))).toBe(true);
    const start = holidays.indexOf(monday);
    expect([0, 1, 2, 3, 4, 5].map((i) => fromDay(holidays.dayAt(start + i)))).toEqual([
      "2026-09-07",
      "2026-09-09",
      "2026-09-10",
      "2026-09-11",
      "2026-09-12",
      "2026-09-14",
    ]);
  });

  it("ignores an exception that matches the weekday rule", () => {
    const same = new WorkingCalendar({
      workingWeekdays: [1, 2, 3, 4, 5],
      exceptions: [{ date: "2026-09-13", working: false }],
    });
    expect(same.indexOf(monday + 14)).toBe(cal.indexOf(monday + 14));
  });

  it("refuses a calendar with no working weekdays", () => {
    expect(() => new WorkingCalendar({ workingWeekdays: [], exceptions: [] })).toThrow(RangeError);
    expect(() => new WorkingCalendar({ workingWeekdays: [7], exceptions: [] })).toThrow(RangeError);
  });

  it("holds its defining properties on random calendars", () => {
    const next = mulberry32(42);
    for (let trial = 0; trial < 50; trial++) {
      const weekdays = [0, 1, 2, 3, 4, 5, 6].filter(() => next() < 0.6);
      if (weekdays.length === 0) weekdays.push(randomInt(next, 0, 6));
      const exceptions = Array.from({ length: randomInt(next, 0, 20) }, () => ({
        date: fromDay(monday + randomInt(next, -60, 60)),
        working: next() < 0.5,
      }));
      const random = new WorkingCalendar({ workingWeekdays: weekdays, exceptions });
      for (let day = monday - 80; day < monday + 80; day++) {
        const step = random.indexOf(day + 1) - random.indexOf(day);
        // indexOf rises by exactly one across a working day and not at all otherwise.
        expect(step).toBe(random.isWorking(day) ? 1 : 0);
        if (random.isWorking(day)) expect(random.dayAt(random.indexOf(day))).toBe(day);
      }
      for (let index = random.indexOf(monday - 50); index < random.indexOf(monday + 50); index++) {
        const day = random.dayAt(index);
        // dayAt never lands on a non-working day, and round-trips.
        expect(random.isWorking(day)).toBe(true);
        expect(random.indexOf(day)).toBe(index);
      }
    }
  });
});
