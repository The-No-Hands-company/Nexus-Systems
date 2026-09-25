import { describe, expect, it } from "bun:test";
import { CycleError, type Edge, findCycle, topologicalOrder } from "../src/schedule/graph";
import { mulberry32, randomInt } from "./support/random";

/** Independent oracle: a graph has a cycle iff some node reaches itself. */
function hasCycleBruteForce(nodes: string[], edges: Edge[]): boolean {
  const index = new Map(nodes.map((n, i) => [n, i]));
  const n = nodes.length;
  const reach = Array.from({ length: n }, () => new Array<boolean>(n).fill(false));
  for (const e of edges)
    (reach[index.get(e.from) as number] as boolean[])[index.get(e.to) as number] = true;
  for (let k = 0; k < n; k++)
    for (let i = 0; i < n; i++)
      for (let j = 0; j < n; j++)
        if (reach[i]?.[k] && reach[k]?.[j]) (reach[i] as boolean[])[j] = true;
  return nodes.some((_, i) => reach[i]?.[i]);
}

function randomGraph(next: () => number): { nodes: string[]; edges: Edge[] } {
  const nodes = Array.from({ length: randomInt(next, 1, 8) }, (_, i) => `n${i}`);
  const edges: Edge[] = [];
  const count = randomInt(next, 0, 12);
  for (let i = 0; i < count; i++) {
    const from = nodes[randomInt(next, 0, nodes.length - 1)] as string;
    const to = nodes[randomInt(next, 0, nodes.length - 1)] as string;
    if (from !== to && !edges.some((e) => e.from === from && e.to === to)) edges.push({ from, to });
  }
  return { nodes, edges };
}

describe("graph", () => {
  it("orders every edge's source before its target", () => {
    const order = topologicalOrder(
      ["a", "b", "c", "d"],
      [
        { from: "c", to: "b" },
        { from: "b", to: "a" },
        { from: "d", to: "a" },
      ],
    );
    const at = (id: string) => order.indexOf(id);
    expect(order).toHaveLength(4);
    expect(at("c") < at("b") && at("b") < at("a") && at("d") < at("a")).toBe(true);
  });

  it("names the cycle it refuses", () => {
    const edges = [
      { from: "a", to: "b" },
      { from: "b", to: "c" },
      { from: "c", to: "a" },
    ];
    expect(() => topologicalOrder(["a", "b", "c"], edges)).toThrow(CycleError);
    try {
      topologicalOrder(["a", "b", "c"], edges);
    } catch (error) {
      expect((error as CycleError).cycle).toEqual(["a", "b", "c", "a"]);
    }
  });

  it("agrees with a brute-force oracle on random graphs", () => {
    const next = mulberry32(99);
    for (let trial = 0; trial < 500; trial++) {
      const { nodes, edges } = randomGraph(next);
      const expected = hasCycleBruteForce(nodes, edges);
      const cycle = findCycle(nodes, edges);
      expect(cycle !== null).toBe(expected);
      if (cycle) {
        // The reported cycle is a real closed path through existing edges.
        expect(cycle[0]).toBe(cycle[cycle.length - 1]);
        for (let i = 0; i + 1 < cycle.length; i++) {
          expect(edges.some((e) => e.from === cycle[i] && e.to === cycle[i + 1])).toBe(true);
        }
      } else {
        const order = topologicalOrder(nodes, edges);
        expect([...order].sort()).toEqual([...nodes].sort());
        for (const e of edges) expect(order.indexOf(e.from) < order.indexOf(e.to)).toBe(true);
      }
    }
  });

  it("rejects an edge to an unknown node", () => {
    expect(() => topologicalOrder(["a"], [{ from: "a", to: "zz" }])).toThrow("unknown node");
  });
});
