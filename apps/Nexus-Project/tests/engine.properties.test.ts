import { describe, expect, it } from "bun:test";
import { WorkingCalendar, toDay } from "../src/schedule/calendar";
import { schedule } from "../src/schedule/engine";
import type { LinkType, ScheduleInput, ScheduleLink, ScheduleTask } from "../src/schedule/types";
import { mulberry32, randomInt } from "./support/random";
import { MONDAY, task } from "./support/schedule";

const TYPES: LinkType[] = ["FS", "SS", "FF", "SF"];

/** A random acyclic project: links only run from a lower to a higher position. */
function randomProject(
  seed: number,
  size: number,
  linkCount: number,
  constrained = false,
): ScheduleInput {
  const next = mulberry32(seed);
  const dateFrom = (offset: number) =>
    new Date((toDay(MONDAY) + offset) * 86_400_000).toISOString().slice(0, 10);
  const weekdays = [1, 2, 3, 4, 5].filter(() => next() < 0.9);
  if (weekdays.length === 0) weekdays.push(3);
  const tasks: ScheduleTask[] = Array.from({ length: size }, (_, i) => {
    const milestone = next() < 0.1;
    const fields: Partial<ScheduleTask> = milestone
      ? { kind: "milestone", durationDays: null }
      : { durationDays: randomInt(next, 1, 8) };
    if (constrained && next() < 0.3) {
      fields.constraintType = "start_no_earlier_than";
      fields.constraintDate = dateFrom(randomInt(next, 0, 40));
    }
    if (constrained && next() < 0.3) fields.deadline = dateFrom(randomInt(next, 0, 60));
    return task(`t${i}`, i + 1, fields);
  });
  const links: ScheduleLink[] = [];
  const seen = new Set<string>();
  for (let i = 0; i < linkCount && size > 1; i++) {
    const a = randomInt(next, 0, size - 2);
    const b = randomInt(next, a + 1, size - 1);
    const key = `${a}-${b}`;
    if (seen.has(key)) continue;
    seen.add(key);
    links.push({
      id: `l${i}`,
      predecessorId: `t${a}`,
      successorId: `t${b}`,
      type: TYPES[randomInt(next, 0, 3)] as LinkType,
      lagDays: randomInt(next, -2, 3),
    });
  }
  const exceptions = Array.from({ length: randomInt(next, 0, 4) }, () => ({
    date: new Date((toDay(MONDAY) + randomInt(next, 0, 60)) * 86_400_000)
      .toISOString()
      .slice(0, 10),
    working: false,
  }));
  return {
    projectStart: MONDAY,
    mode: "auto",
    calendar: { workingWeekdays: weekdays, exceptions },
    tasks,
    links,
  };
}

function positions(input: ScheduleInput) {
  const calendar = new WorkingCalendar(input.calendar);
  const result = schedule(input);
  const duration = new Map(
    input.tasks.map((t) => [t.id, t.kind === "milestone" ? 0 : (t.durationDays as number)]),
  );
  const at = new Map(
    result.tasks.map((t) => {
      const d = duration.get(t.id) as number;
      const es = calendar.indexOf(toDay(t.earlyStart));
      const ls = calendar.indexOf(toDay(t.lateStart));
      return [t.id, { es, ef: es + d, ls, lf: ls + d, tf: t.totalFloat, critical: t.critical }];
    }),
  );
  return { result, at, calendar, start: calendar.indexOf(toDay(input.projectStart)) };
}

interface Position {
  es: number;
  ef: number;
  ls: number;
  lf: number;
  tf: number;
  critical: boolean;
}

/**
 * The backward pass, checked in mirror form: a predecessor placed at its late
 * dates must still let its successor start at the successor's late start, and
 * nothing may finish late after the project's early finish.
 */
function expectLateDatesConsistent(project: ScheduleInput, at: Map<string, Position>): void {
  const finish = Math.max(...[...at.values()].map((p) => p.ef));
  for (const link of project.links) {
    const pred = at.get(link.predecessorId) as Position;
    const succ = at.get(link.successorId) as Position;
    expect(required(link, { es: pred.ls, ef: pred.lf }, succ.lf - succ.ls)).toBeLessThanOrEqual(
      succ.ls,
    );
  }
  for (const p of at.values()) expect(p.lf).toBeLessThanOrEqual(finish);
}

function required(
  link: ScheduleLink,
  pred: { es: number; ef: number },
  successorDuration: number,
): number {
  const base = link.type === "FS" || link.type === "FF" ? pred.ef : pred.es;
  const shift = link.type === "FF" || link.type === "SF" ? successorDuration : 0;
  return base + link.lagDays - shift;
}

describe("engine properties on random projects", () => {
  for (let seed = 1; seed <= 200; seed++) {
    const project = randomProject(seed, 2 + (seed % 25), seed % 40);
    const { result, at, start } = positions(project);

    it(`seed ${seed}: satisfies every link and never starts before the project`, () => {
      for (const link of project.links) {
        const pred = at.get(link.predecessorId);
        const succ = at.get(link.successorId);
        if (!pred || !succ) throw new Error("a linked task was not scheduled");
        expect(succ.es).toBeGreaterThanOrEqual(required(link, pred, succ.ef - succ.es));
      }
      for (const p of at.values()) expect(p.es).toBeGreaterThanOrEqual(start);
    });

    it(`seed ${seed}: has no negative float without deadlines, and the last finish is critical`, () => {
      const finish = Math.max(...[...at.values()].map((p) => p.ef));
      for (const p of at.values()) {
        expect(p.tf).toBeGreaterThanOrEqual(0);
        if (p.ef === finish) expect(p.critical).toBe(true);
      }
    });

    it(`seed ${seed}: every critical task after the start is driven by a critical predecessor`, () => {
      for (const [id, p] of at) {
        if (!p.critical || p.es === start) continue;
        const drivers = project.links.filter((l) => {
          if (l.successorId !== id) return false;
          const pred = at.get(l.predecessorId) as { es: number; ef: number; critical: boolean };
          return pred.critical && required(l, pred, p.ef - p.es) === p.es;
        });
        expect(drivers.length).toBeGreaterThan(0);
      }
    });

    it(`seed ${seed}: every early start is minimal — the project start or a binding link`, () => {
      for (const t of project.tasks) {
        const p = at.get(t.id) as Position;
        let earliest = start;
        for (const l of project.links) {
          if (l.successorId !== t.id) continue;
          earliest = Math.max(
            earliest,
            required(l, at.get(l.predecessorId) as Position, p.ef - p.es),
          );
        }
        expect(p.es).toBe(earliest);
      }
    });

    it(`seed ${seed}: late dates satisfy every link and never pass the project finish`, () => {
      expectLateDatesConsistent(project, at);
    });

    it(`seed ${seed}: a manual plan that already matches the logic stays put with no violations`, () => {
      const byId = new Map(result.tasks.map((t) => [t.id, t]));
      const manual: ScheduleInput = {
        ...project,
        mode: "manual",
        tasks: project.tasks.map((t) => ({ ...t, startDate: byId.get(t.id)?.earlyStart ?? null })),
      };
      const again = schedule(manual);
      expect(again.violations).toEqual([]);
      expect(again.tasks.map((t) => t.earlyStart)).toEqual(result.tasks.map((t) => t.earlyStart));
    });
  }

  it("links bind late dates on many seeds, so the late-date property is not vacuous", () => {
    // Guard against the properties above passing vacuously: some seed must
    // exercise late dates that a link actually constrains.
    let constrainedLinks = 0;
    for (let seed = 1; seed <= 200; seed++) {
      const project = randomProject(seed, 2 + (seed % 25), seed % 40);
      const { at } = positions(project);
      for (const l of project.links) {
        const pred = at.get(l.predecessorId) as Position;
        const succ = at.get(l.successorId) as Position;
        const late = { es: pred.ls, ef: pred.lf };
        if (required(l, late, succ.lf - succ.ls) === succ.ls) constrainedLinks++;
      }
    }
    expect(constrainedLinks).toBeGreaterThan(100);
  });

  it("gives the same answer whatever order tasks and links arrive in", () => {
    const project = randomProject(4242, 30, 60);
    const next = mulberry32(1);
    const shuffle = <T>(items: readonly T[]) => [...items].sort(() => next() - 0.5);
    const shuffled = { ...project, tasks: shuffle(project.tasks), links: shuffle(project.links) };
    expect(schedule(shuffled)).toEqual(schedule(project));
  });
});

describe("engine properties with start constraints and deadlines", () => {
  for (let seed = 1; seed <= 200; seed++) {
    const project = randomProject(seed * 7919, 2 + (seed % 25), seed % 40, true);
    const { at, calendar, start } = positions(project);

    it(`seed ${seed}: satisfies every link, the project start and every start-no-earlier-than`, () => {
      for (const link of project.links) {
        const pred = at.get(link.predecessorId) as Position;
        const succ = at.get(link.successorId) as Position;
        expect(succ.es).toBeGreaterThanOrEqual(required(link, pred, succ.ef - succ.es));
      }
      for (const t of project.tasks) {
        const p = at.get(t.id) as Position;
        expect(p.es).toBeGreaterThanOrEqual(start);
        if (t.constraintType === "start_no_earlier_than" && t.constraintDate !== null)
          expect(p.es).toBeGreaterThanOrEqual(calendar.indexOf(toDay(t.constraintDate)));
      }
    });

    it(`seed ${seed}: late dates satisfy every link, the project finish and every deadline`, () => {
      expectLateDatesConsistent(project, at);
      for (const t of project.tasks) {
        if (t.deadline === null) continue;
        // The deadline day itself is still usable: the cap is the start of the next working day.
        const cap = calendar.indexOf(toDay(t.deadline) + 1);
        expect((at.get(t.id) as Position).lf).toBeLessThanOrEqual(cap);
      }
    });
  }

  it("the family really carries constraints and deadlines that bind", () => {
    let negative = 0;
    for (let seed = 1; seed <= 200; seed++) {
      const project = randomProject(seed * 7919, 2 + (seed % 25), seed % 40, true);
      negative += positions(project).result.tasks.filter((t) => t.totalFloat < 0).length;
    }
    expect(negative).toBeGreaterThan(0);
  });
});

describe("engine performance", () => {
  it("schedules 5,000 tasks and 10,000 links in under 50 ms", () => {
    const project = randomProject(2026, 5000, 10_000);
    schedule(project); // warm up the JIT
    const runs = [0, 1, 2, 3, 4].map(() => {
      const started = performance.now();
      schedule(project);
      return performance.now() - started;
    });
    const median = [...runs].sort((a, b) => a - b)[2] as number;
    expect(median).toBeLessThan(50);
  });
});
