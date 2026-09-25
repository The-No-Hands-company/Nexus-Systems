import { describe, expect, it } from "bun:test";
import { WorkingCalendar, toDay } from "../src/schedule/calendar";
import { schedule } from "../src/schedule/engine";
import type { LinkType, ScheduleInput, ScheduleLink, ScheduleTask } from "../src/schedule/types";
import { mulberry32, randomInt } from "./support/random";
import { MONDAY, task } from "./support/schedule";

const TYPES: LinkType[] = ["FS", "SS", "FF", "SF"];

/** A random acyclic project: links only run from a lower to a higher position. */
function randomProject(seed: number, size: number, linkCount: number): ScheduleInput {
  const next = mulberry32(seed);
  const weekdays = [1, 2, 3, 4, 5].filter(() => next() < 0.9);
  if (weekdays.length === 0) weekdays.push(3);
  const tasks: ScheduleTask[] = Array.from({ length: size }, (_, i) => {
    const milestone = next() < 0.1;
    return task(
      `t${i}`,
      i + 1,
      milestone
        ? { kind: "milestone", durationDays: null }
        : { durationDays: randomInt(next, 1, 8) },
    );
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
      const es = calendar.indexOf(toDay(t.earlyStart));
      return [
        t.id,
        { es, ef: es + (duration.get(t.id) as number), tf: t.totalFloat, critical: t.critical },
      ];
    }),
  );
  return { result, at, start: calendar.indexOf(toDay(input.projectStart)) };
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

  it("gives the same answer whatever order tasks and links arrive in", () => {
    const project = randomProject(4242, 30, 60);
    const next = mulberry32(1);
    const shuffle = <T>(items: readonly T[]) => [...items].sort(() => next() - 0.5);
    const shuffled = { ...project, tasks: shuffle(project.tasks), links: shuffle(project.links) };
    expect(schedule(shuffled)).toEqual(schedule(project));
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
