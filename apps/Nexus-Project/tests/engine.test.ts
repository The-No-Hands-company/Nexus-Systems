import { describe, expect, it } from "bun:test";
import { schedule } from "../src/schedule/engine";
import { CycleError } from "../src/schedule/graph";
import { byId, get, input, link, task } from "./support/schedule";

// Dates below: 2026-09-07 Mon … 09-11 Fri, 09-14 Mon … 09-18 Fri.

describe("forward pass and links", () => {
  it("chains finish-to-start and marks the chain critical", () => {
    const r = schedule(input([task("A", 1, { durationDays: 3 }), task("B", 2, { durationDays: 2 })], [link("l", "A", "B")]));
    expect(get(r, "A")).toMatchObject({ earlyStart: "2026-09-07", earlyFinish: "2026-09-09", totalFloat: 0, critical: true });
    expect(get(r, "B")).toMatchObject({ earlyStart: "2026-09-10", earlyFinish: "2026-09-11", totalFloat: 0, critical: true });
    expect(r.projectFinish).toBe("2026-09-11");
    expect(r.criticalPath).toEqual(["A", "B"]);
  });

  it("skips the weekend", () => {
    const r = schedule(input([task("A", 1, { durationDays: 5 }), task("B", 2)], [link("l", "A", "B")]));
    expect(get(r, "A").earlyFinish).toBe("2026-09-11");
    expect(get(r, "B").earlyStart).toBe("2026-09-14");
  });

  it("applies lag and lead in working days", () => {
    const lag = schedule(input([task("A", 1, { durationDays: 3 }), task("B", 2)], [link("l", "A", "B", "FS", 2)]));
    expect(get(lag, "B").earlyStart).toBe("2026-09-14");
    const lead = schedule(input([task("A", 1, { durationDays: 3 }), task("B", 2)], [link("l", "A", "B", "FS", -1)]));
    expect(get(lead, "B").earlyStart).toBe("2026-09-09");
  });

  it("schedules start-to-start", () => {
    const r = schedule(input([task("A", 1, { durationDays: 3 }), task("B", 2, { durationDays: 2 })], [link("l", "A", "B", "SS", 1)]));
    expect(get(r, "B")).toMatchObject({ earlyStart: "2026-09-08", earlyFinish: "2026-09-09", critical: true });
    expect(get(r, "A").critical).toBe(true);
  });

  it("schedules finish-to-finish", () => {
    const r = schedule(input([task("A", 1, { durationDays: 3 }), task("B", 2)], [link("l", "A", "B", "FF")]));
    expect(get(r, "B")).toMatchObject({ earlyStart: "2026-09-09", earlyFinish: "2026-09-09" });
  });

  it("schedules start-to-finish", () => {
    const tasks = [
      task("A", 1, { durationDays: 3, constraintType: "start_no_earlier_than", constraintDate: "2026-09-14" }),
      task("B", 2, { durationDays: 2 }),
    ];
    const r = schedule(input(tasks, [link("l", "A", "B", "SF")]));
    expect(get(r, "A").earlyStart).toBe("2026-09-14");
    expect(get(r, "B")).toMatchObject({ earlyStart: "2026-09-10", earlyFinish: "2026-09-11" });
  });

  it("respects a holiday", () => {
    const r = schedule(
      input([task("A", 1, { durationDays: 3 })], [], {
        calendar: { workingWeekdays: [1, 2, 3, 4, 5], exceptions: [{ date: "2026-09-08", working: false }] },
      }),
    );
    expect(get(r, "A").earlyFinish).toBe("2026-09-10");
  });

  it("places a milestone at the start of the day its driver frees", () => {
    const r = schedule(input([task("A", 1, { durationDays: 3 }), task("M", 2, { kind: "milestone", durationDays: null })], [link("l", "A", "M")]));
    expect(get(r, "M")).toMatchObject({ earlyStart: "2026-09-10", earlyFinish: "2026-09-10", critical: true });
  });

  it("refuses a cycle", () => {
    expect(() => schedule(input([task("A", 1), task("B", 2)], [link("x", "A", "B"), link("y", "B", "A")]))).toThrow(CycleError);
  });
});

describe("backward pass and float", () => {
  it("gives the shorter parallel path float", () => {
    const r = schedule(
      input(
        [task("A", 1, { durationDays: 5 }), task("B", 2, { durationDays: 2 }), task("C", 3)],
        [link("l1", "A", "C"), link("l2", "B", "C")],
      ),
    );
    expect(get(r, "A")).toMatchObject({ totalFloat: 0, freeFloat: 0, critical: true });
    expect(get(r, "B")).toMatchObject({ totalFloat: 3, freeFloat: 3, critical: false, lateStart: "2026-09-10" });
    expect(r.criticalPath).toEqual(["A", "C"]);
  });

  it("turns a missed deadline into negative float", () => {
    const r = schedule(input([task("A", 1, { durationDays: 5, deadline: "2026-09-09" })]));
    expect(get(r, "A")).toMatchObject({ totalFloat: -2, critical: true, lateFinish: "2026-09-09", lateStart: "2026-09-03" });
  });
});

describe("modes", () => {
  it("manual mode never moves a stored date earlier and reports the broken link", () => {
    const r = schedule(
      input(
        [task("A", 1, { durationDays: 3, startDate: "2026-09-07" }), task("B", 2, { durationDays: 2, startDate: "2026-09-08" })],
        [link("l", "A", "B")],
        { mode: "manual" },
      ),
    );
    expect(r.violations).toEqual([{ linkId: "l", predecessorId: "A", successorId: "B", type: "FS", gapDays: 2 }]);
    expect(get(r, "B").earlyStart).toBe("2026-09-10");
  });

  it("manual mode keeps a stored date later than the logic needs", () => {
    const r = schedule(
      input(
        [task("A", 1, { durationDays: 3, startDate: "2026-09-07" }), task("B", 2, { durationDays: 2, startDate: "2026-09-21" })],
        [link("l", "A", "B")],
        { mode: "manual" },
      ),
    );
    expect(r.violations).toEqual([]);
    expect(get(r, "B").earlyStart).toBe("2026-09-21");
    expect(get(r, "A").totalFloat).toBe(7);
  });

  it("auto mode ignores stored dates and honours start-no-earlier-than", () => {
    const r = schedule(
      input([
        task("A", 1, { durationDays: 3 }),
        task("B", 2, { durationDays: 2, startDate: "2026-09-21" }),
        task("C", 3, { constraintType: "start_no_earlier_than", constraintDate: "2026-09-09" }),
      ], [link("l", "A", "B")]),
    );
    expect(get(r, "B").earlyStart).toBe("2026-09-10");
    expect(get(r, "C").earlyStart).toBe("2026-09-09");
    expect(r.violations).toEqual([]);
  });

  it("pins a completed task to its recorded start", () => {
    const r = schedule(
      input([task("A", 1, { durationDays: 3 }), task("C", 2, { state: "completed", startDate: "2026-09-07" })], [link("l", "A", "C")]),
    );
    expect(get(r, "C").earlyStart).toBe("2026-09-07");
    expect(r.violations).toEqual([]);
  });
});

describe("tasks outside the schedule", () => {
  it("ignores links to unestimated and canceled tasks and says so", () => {
    const r = schedule(
      input(
        [task("A", 1), task("U", 2, { durationDays: null }), task("X", 3, { state: "canceled", durationDays: 2 })],
        [link("l1", "A", "U"), link("l2", "X", "A")],
      ),
    );
    expect([...byId(r).keys()]).toEqual(["A"]);
    expect(r.warnings).toEqual([
      { code: "link_ignored", linkId: "l1", taskId: "U", reason: "unestimated" },
      { code: "link_ignored", linkId: "l2", taskId: "X", reason: "canceled" },
    ]);
  });

  it("returns an empty schedule for a project with nothing estimated", () => {
    const r = schedule(input([task("U", 1, { durationDays: null })]));
    expect(r).toMatchObject({ projectFinish: null, tasks: [], criticalPath: [] });
  });
});

describe("summary roll-up", () => {
  it("spans its children and weights progress by duration", () => {
    const r = schedule(
      input(
        [
          task("S", 1, { durationDays: null }),
          task("A", 2, { parentId: "S", durationDays: 3, progress: 50 }),
          task("B", 3, { parentId: "S", durationDays: 1, progress: 0 }),
        ],
        [link("l", "A", "B")],
      ),
    );
    expect(r.summaries).toEqual([{ id: "S", start: "2026-09-07", finish: "2026-09-10", progress: 38 }]);
    expect(byId(r).has("S")).toBe(false);
  });

  it("rolls up through nested summaries and counts unestimated work", () => {
    const r = schedule(
      input([
        task("S1", 1, { durationDays: null }),
        task("S2", 2, { parentId: "S1", durationDays: null }),
        task("A", 3, { parentId: "S2", durationDays: 2, state: "completed" }),
        task("U", 4, { parentId: "S1", durationDays: null }),
        task("X", 5, { parentId: "S1", durationDays: 9, state: "canceled" }),
      ]),
    );
    const summaries = new Map(r.summaries.map((s) => [s.id, s]));
    expect(summaries.get("S2")).toEqual({ id: "S2", start: "2026-09-07", finish: "2026-09-08", progress: 100 });
    expect(summaries.get("S1")).toEqual({ id: "S1", start: "2026-09-07", finish: "2026-09-08", progress: 67 });
  });
});
