import { WorkingCalendar, fromDay, toDay } from "./calendar";
import { type Edge, topologicalOrder } from "./graph";
import type {
  ScheduleInput,
  ScheduleLink,
  ScheduleResult,
  ScheduleTask,
  ScheduleWarning,
  SummaryRollup,
  TaskSchedule,
  Violation,
} from "./types";

/** A scheduled leaf. All positions are working-day indices; [es, ef) is half-open. */
interface Node {
  task: ScheduleTask;
  duration: number;
  floor: number;
  stored: number | null; // manual mode only
  pinned: number | null; // completed with a recorded start
  deadlineCap: number | null;
  es: number;
  ef: number;
  ls: number;
  lf: number;
}

/** The earliest start a link allows its successor. */
function requiredStart(link: ScheduleLink, pred: Node, successorDuration: number): number {
  switch (link.type) {
    case "FS":
      return pred.ef + link.lagDays;
    case "SS":
      return pred.es + link.lagDays;
    case "FF":
      return pred.ef + link.lagDays - successorDuration;
    case "SF":
      return pred.es + link.lagDays - successorDuration;
  }
}

/** The latest finish a link allows its predecessor. */
function latestFinish(link: ScheduleLink, succ: Node, predecessorDuration: number): number {
  switch (link.type) {
    case "FS":
      return succ.ls - link.lagDays;
    case "SS":
      return succ.ls - link.lagDays + predecessorDuration;
    case "FF":
      return succ.lf - link.lagDays;
    case "SF":
      return succ.lf - link.lagDays + predecessorDuration;
  }
}

/** How far the predecessor could slip, on early dates, before this link binds. */
function linkSlack(link: ScheduleLink, pred: Node, succ: Node): number {
  switch (link.type) {
    case "FS":
      return succ.es - (pred.ef + link.lagDays);
    case "SS":
      return succ.es - (pred.es + link.lagDays);
    case "FF":
      return succ.ef - (pred.ef + link.lagDays);
    case "SF":
      return succ.ef - (pred.es + link.lagDays);
  }
}

function push<K, V>(map: Map<K, V[]>, key: K, value: V): void {
  const list = map.get(key);
  if (list) list.push(value);
  else map.set(key, [value]);
}

interface Aggregate {
  start: string | null;
  finish: string | null;
  weight: number;
  weighted: number;
  count: number;
  sum: number;
}

export function schedule(input: ScheduleInput): ScheduleResult {
  const calendar = new WorkingCalendar(input.calendar);
  const indexOf = (date: string) => calendar.indexOf(toDay(date));
  const dateAt = (index: number) => fromDay(calendar.dayAt(index));
  const projectStart = indexOf(input.projectStart);

  const childrenOf = new Map<string, ScheduleTask[]>();
  for (const task of input.tasks) {
    if (task.parentId === null) continue;
    const siblings = childrenOf.get(task.parentId) ?? [];
    siblings.push(task);
    childrenOf.set(task.parentId, siblings);
  }

  const nodes = new Map<string, Node>();
  const excluded = new Map<string, "unestimated" | "canceled">();
  for (const task of input.tasks) {
    if (childrenOf.has(task.id)) continue; // summary: scheduled by its children
    if (task.state === "canceled") {
      excluded.set(task.id, "canceled");
      continue;
    }
    const duration = task.kind === "milestone" ? 0 : task.durationDays;
    if (duration === null) {
      excluded.set(task.id, "unestimated");
      continue;
    }
    const recorded = task.startDate === null ? null : indexOf(task.startDate);
    const stored = input.mode === "manual" ? recorded : null;
    let floor = stored ?? projectStart;
    if (task.constraintType === "start_no_earlier_than" && task.constraintDate !== null) {
      floor = Math.max(floor, indexOf(task.constraintDate));
    }
    nodes.set(task.id, {
      task,
      duration,
      floor,
      stored,
      pinned: task.state === "completed" ? recorded : null,
      deadlineCap: task.deadline === null ? null : calendar.indexOf(toDay(task.deadline) + 1),
      es: 0,
      ef: 0,
      ls: 0,
      lf: 0,
    });
  }

  const incoming = new Map<string, ScheduleLink[]>();
  const outgoing = new Map<string, ScheduleLink[]>();
  const edges: Edge[] = [];
  const warnings: ScheduleWarning[] = [];
  for (const link of input.links) {
    if (nodes.has(link.predecessorId) && nodes.has(link.successorId)) {
      push(incoming, link.successorId, link);
      push(outgoing, link.predecessorId, link);
      edges.push({ from: link.predecessorId, to: link.successorId });
      continue;
    }
    for (const taskId of [link.predecessorId, link.successorId]) {
      const reason = excluded.get(taskId);
      if (reason) {
        warnings.push({ code: "link_ignored", linkId: link.id, taskId, reason });
        break;
      }
    }
  }

  const ids = [...nodes.values()].sort((a, b) => a.task.number - b.task.number).map((n) => n.task.id);
  const order = topologicalOrder(ids, edges);
  const node = (id: string) => nodes.get(id) as Node;

  // Forward pass.
  const violations: Violation[] = [];
  for (const id of order) {
    const current = node(id);
    let es = current.floor;
    if (current.pinned !== null) {
      es = current.pinned;
    } else {
      for (const link of incoming.get(id) ?? []) {
        const required = requiredStart(link, node(link.predecessorId), current.duration);
        if (current.stored !== null && required > current.stored) {
          violations.push({
            linkId: link.id,
            predecessorId: link.predecessorId,
            successorId: id,
            type: link.type,
            gapDays: required - current.stored,
          });
        }
        es = Math.max(es, required);
      }
    }
    current.es = es;
    current.ef = es + current.duration;
  }

  const projectFinish = order.length === 0 ? null : Math.max(...order.map((id) => node(id).ef));

  // Backward pass.
  for (let i = order.length - 1; i >= 0; i--) {
    const current = node(order[i] as string);
    let lf = projectFinish as number;
    if (current.deadlineCap !== null) lf = Math.min(lf, current.deadlineCap);
    for (const link of outgoing.get(current.task.id) ?? []) {
      lf = Math.min(lf, latestFinish(link, node(link.successorId), current.duration));
    }
    current.lf = lf;
    current.ls = lf - current.duration;
  }

  const finishDate = (start: number, end: number) => (end > start ? dateAt(end - 1) : dateAt(start));
  const tasks: TaskSchedule[] = ids.map((id) => {
    const current = node(id);
    const totalFloat = current.ls - current.es;
    let freeFloat = (projectFinish as number) - current.ef;
    if (current.deadlineCap !== null) freeFloat = Math.min(freeFloat, current.deadlineCap - current.ef);
    for (const link of outgoing.get(id) ?? []) {
      freeFloat = Math.min(freeFloat, linkSlack(link, current, node(link.successorId)));
    }
    return {
      id,
      earlyStart: dateAt(current.es),
      earlyFinish: finishDate(current.es, current.ef),
      lateStart: dateAt(current.ls),
      lateFinish: finishDate(current.ls, current.lf),
      totalFloat,
      freeFloat: Math.min(freeFloat, totalFloat),
      critical: totalFloat <= 0,
    };
  });

  const criticalPath = tasks
    .filter((t) => t.critical)
    .sort((a, b) => node(a.id).es - node(b.id).es || node(a.id).task.number - node(b.id).task.number)
    .map((t) => t.id);

  return {
    projectFinish: tasks.reduce<string | null>((max, t) => (max === null || t.earlyFinish > max ? t.earlyFinish : max), null),
    tasks,
    summaries: rollUp(input.tasks, childrenOf, nodes, new Map(tasks.map((t) => [t.id, t]))),
    criticalPath,
    violations,
    warnings,
  };
}

function rollUp(
  all: readonly ScheduleTask[],
  childrenOf: Map<string, ScheduleTask[]>,
  nodes: Map<string, Node>,
  scheduled: Map<string, TaskSchedule>,
): SummaryRollup[] {
  const memo = new Map<string, Aggregate>();
  const aggregate = (task: ScheduleTask): Aggregate => {
    const cached = memo.get(task.id);
    if (cached) return cached;
    const children = childrenOf.get(task.id);
    let result: Aggregate;
    if (children) {
      result = { start: null, finish: null, weight: 0, weighted: 0, count: 0, sum: 0 };
      for (const child of children) {
        if (child.state === "canceled" && !childrenOf.has(child.id)) continue;
        const part = aggregate(child);
        if (part.start !== null && (result.start === null || part.start < result.start)) result.start = part.start;
        if (part.finish !== null && (result.finish === null || part.finish > result.finish)) result.finish = part.finish;
        result.weight += part.weight;
        result.weighted += part.weighted;
        result.count += part.count;
        result.sum += part.sum;
      }
    } else {
      const done = task.state === "completed";
      const timing = scheduled.get(task.id);
      const leaf = nodes.get(task.id);
      const progress = leaf ? (done ? 100 : task.progress) : done ? 100 : 0;
      const weight = leaf ? leaf.duration : 1;
      result = {
        start: timing?.earlyStart ?? null,
        finish: timing?.earlyFinish ?? null,
        weight,
        weighted: weight * progress,
        count: 1,
        sum: progress,
      };
    }
    memo.set(task.id, result);
    return result;
  };

  const summaries: SummaryRollup[] = [];
  for (const task of all) {
    if (!childrenOf.has(task.id)) continue;
    const total = aggregate(task);
    const progress = total.weight > 0 ? total.weighted / total.weight : total.count > 0 ? total.sum / total.count : 0;
    summaries.push({ id: task.id, start: total.start, finish: total.finish, progress: Math.round(progress) });
  }
  return summaries;
}
