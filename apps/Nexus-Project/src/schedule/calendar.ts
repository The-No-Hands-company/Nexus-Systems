import type { CalendarSpec } from "./types";

export const DAY_MS = 86_400_000;
const DATE = /^(\d{4})-(\d{2})-(\d{2})$/;

/** Days since 1970-01-01 (UTC). Only real YYYY-MM-DD dates are accepted. */
export function toDay(date: string): number {
  const match = DATE.exec(date);
  if (!match) throw new RangeError(`not a YYYY-MM-DD date: ${date}`);
  const day = Date.UTC(Number(match[1]), Number(match[2]) - 1, Number(match[3])) / DAY_MS;
  if (fromDay(day) !== date) throw new RangeError(`not a real calendar date: ${date}`);
  return day;
}

export function fromDay(day: number): string {
  return new Date(day * DAY_MS).toISOString().slice(0, 10);
}

/** 0 = Sunday … 6 = Saturday. Day 0 (1970-01-01) was a Thursday. */
export function weekday(day: number): number {
  return (((day + 4) % 7) + 7) % 7;
}

/**
 * Converts between calendar days and working-day indices.
 *
 * `indexOf(day)` counts the working days in [0, day), so it rises by exactly
 * one across each working day. A non-working day therefore shares its index
 * with the next working day, which is what "a task starting on Saturday
 * starts on Monday" means.
 */
export class WorkingCalendar {
  private readonly mask: number;
  private readonly perWeek: number;
  private readonly flipDays: number[];
  private readonly flipPrefix: number[];
  private readonly flips: Map<number, number>;

  constructor(spec: CalendarSpec) {
    let mask = 0;
    for (const day of spec.workingWeekdays) {
      if (!Number.isInteger(day) || day < 0 || day > 6)
        throw new RangeError(`weekday out of range: ${day}`);
      mask |= 1 << day;
    }
    if (mask === 0) throw new RangeError("a calendar needs at least one working weekday");
    this.mask = mask;
    this.perWeek = [0, 1, 2, 3, 4, 5, 6].filter((d) => (mask >> d) & 1).length;

    // An exception only matters where it disagrees with the weekday rule; it
    // then "flips" that one day. Later entries for the same date win.
    this.flips = new Map();
    for (const exception of spec.exceptions) {
      const day = toDay(exception.date);
      if (exception.working === this.weekdayWorking(day)) this.flips.delete(day);
      else this.flips.set(day, exception.working ? 1 : -1);
    }
    this.flipDays = [...this.flips.keys()].sort((a, b) => a - b);
    this.flipPrefix = [];
    let sum = 0;
    for (const day of this.flipDays) {
      sum += this.flips.get(day) as number;
      this.flipPrefix.push(sum);
    }
  }

  private weekdayWorking(day: number): boolean {
    return ((this.mask >> weekday(day)) & 1) === 1;
  }

  isWorking(day: number): boolean {
    const flip = this.flips.get(day);
    return flip === undefined ? this.weekdayWorking(day) : flip === 1;
  }

  indexOf(day: number): number {
    const weeks = Math.floor(day / 7);
    let count = weeks * this.perWeek;
    for (let d = weeks * 7; d < day; d++) if (this.weekdayWorking(d)) count++;
    return count + this.flipsBefore(day);
  }

  /** The working day whose index is `index`. */
  dayAt(index: number): number {
    // The answer is the smallest day d with indexOf(d + 1) > index. Bracket it
    // from an estimate, widening geometrically, then binary-search.
    const above = (day: number) => this.indexOf(day + 1) > index;
    let lo = Math.floor((index * 7) / this.perWeek) - 7;
    for (let step = 7; above(lo); step *= 2) lo -= step;
    let hi = lo + 7;
    for (let step = 7; !above(hi); step *= 2) hi += step;
    while (hi - lo > 1) {
      const mid = Math.floor((lo + hi) / 2);
      if (above(mid)) hi = mid;
      else lo = mid;
    }
    return hi;
  }

  private flipsBefore(day: number): number {
    let lo = 0;
    let hi = this.flipDays.length;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if ((this.flipDays[mid] as number) < day) lo = mid + 1;
      else hi = mid;
    }
    return lo === 0 ? 0 : (this.flipPrefix[lo - 1] as number);
  }
}
