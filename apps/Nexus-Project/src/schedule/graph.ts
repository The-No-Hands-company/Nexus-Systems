export interface Edge {
  from: string;
  to: string;
}

export class CycleError extends Error {
  constructor(readonly cycle: string[]) {
    super(`dependency cycle: ${cycle.join(" -> ")}`);
  }
}

function adjacency(nodes: readonly string[], edges: readonly Edge[]): Map<string, string[]> {
  const out = new Map<string, string[]>();
  for (const node of nodes) out.set(node, []);
  for (const edge of edges) {
    const targets = out.get(edge.from);
    if (!targets || !out.has(edge.to))
      throw new Error(`edge references unknown node: ${edge.from} -> ${edge.to}`);
    targets.push(edge.to);
  }
  return out;
}

/** Kahn's algorithm. Deterministic for a given input order. Throws CycleError. */
export function topologicalOrder(nodes: readonly string[], edges: readonly Edge[]): string[] {
  const out = adjacency(nodes, edges);
  const indegree = new Map<string, number>(nodes.map((n) => [n, 0]));
  for (const edge of edges) indegree.set(edge.to, (indegree.get(edge.to) as number) + 1);
  const queue = nodes.filter((n) => indegree.get(n) === 0);
  for (let head = 0; head < queue.length; head++) {
    for (const target of out.get(queue[head] as string) as string[]) {
      const remaining = (indegree.get(target) as number) - 1;
      indegree.set(target, remaining);
      if (remaining === 0) queue.push(target);
    }
  }
  if (queue.length !== nodes.length) throw new CycleError(findCycle(nodes, edges) ?? []);
  return queue;
}

/** Iterative depth-first search; returns the first cycle found as [a, …, a]. */
export function findCycle(nodes: readonly string[], edges: readonly Edge[]): string[] | null {
  const out = adjacency(nodes, edges);
  const state = new Map<string, 1 | 2>(); // 1 = on the current path, 2 = finished
  for (const start of nodes) {
    if (state.has(start)) continue;
    const stack: { node: string; next: number }[] = [{ node: start, next: 0 }];
    const path: string[] = [start];
    state.set(start, 1);
    while (stack.length > 0) {
      const frame = stack[stack.length - 1] as { node: string; next: number };
      const targets = out.get(frame.node) as string[];
      if (frame.next < targets.length) {
        const target = targets[frame.next++] as string;
        const seen = state.get(target);
        if (seen === 1) return [...path.slice(path.indexOf(target)), target];
        if (seen === undefined) {
          state.set(target, 1);
          stack.push({ node: target, next: 0 });
          path.push(target);
        }
      } else {
        state.set(frame.node, 2);
        stack.pop();
        path.pop();
      }
    }
  }
  return null;
}
