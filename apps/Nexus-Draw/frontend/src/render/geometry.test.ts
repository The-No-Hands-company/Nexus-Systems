import { describe, it, expect } from "vitest";
import { makeElement, type ElementData } from "../stores/model";
import { elementBounds, shapeEdgePoint, orthogonalBetween, routeConnector } from "./geometry";

describe("shapeEdgePoint", () => {
  it("returns the right-edge point when the target is to the right", () => {
    const b = { x: 0, y: 0, width: 100, height: 50 };
    const p = shapeEdgePoint(b, { x: 200, y: 25 });
    expect(p.x).toBeCloseTo(100, 8);
    expect(p.y).toBeCloseTo(25, 8);
  });
  it("returns the bottom-edge point when the target is below", () => {
    const b = { x: 0, y: 0, width: 100, height: 50 };
    const p = shapeEdgePoint(b, { x: 50, y: 200 });
    expect(p.x).toBeCloseTo(50, 8);
    expect(p.y).toBeCloseTo(50, 8);
  });
  it("falls back to the center when the target is the center", () => {
    const b = { x: 10, y: 20, width: 100, height: 50 };
    expect(shapeEdgePoint(b, { x: 60, y: 45 })).toEqual({ x: 60, y: 45 });
  });
});

describe("orthogonalBetween", () => {
  it("routes horizontally-primary with a mid-x S", () => {
    const pts = orthogonalBetween({ x: 0, y: 0 }, { x: 100, y: 40 });
    expect(pts).toEqual([
      { x: 0, y: 0 },
      { x: 50, y: 0 },
      { x: 50, y: 40 },
      { x: 100, y: 40 },
    ]);
  });
  it("routes vertically-primary with a mid-y S", () => {
    const pts = orthogonalBetween({ x: 0, y: 0 }, { x: 40, y: 100 });
    expect(pts).toEqual([
      { x: 0, y: 0 },
      { x: 0, y: 50 },
      { x: 40, y: 50 },
      { x: 40, y: 100 },
    ]);
  });
  it("keeps a straight line when already axis-aligned", () => {
    expect(orthogonalBetween({ x: 0, y: 0 }, { x: 0, y: 60 })).toEqual([{ x: 0, y: 0 }, { x: 0, y: 60 }]);
  });
});

describe("routeConnector", () => {
  const boxA = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
  const boxB = makeElement("rectangle", { x: 300, y: 0, width: 100, height: 50 });
  const elements = [boxA, boxB];

  it("routes a free straight connector between its points", () => {
    const el = makeElement("connector", { startPoint: { x: 0, y: 0 }, endPoint: { x: 100, y: 50 }, routing: "straight" });
    const pts = routeConnector(el, elements);
    expect(pts).toEqual([{ x: 0, y: 0 }, { x: 100, y: 50 }]);
  });

  it("routes a free elbow connector with orthogonal bends", () => {
    const el = makeElement("connector", { startPoint: { x: 0, y: 0 }, endPoint: { x: 100, y: 40 }, routing: "elbow" });
    const pts = routeConnector(el, elements);
    expect(pts[0]).toEqual({ x: 0, y: 0 });
    expect(pts[pts.length - 1]).toEqual({ x: 100, y: 40 });
    // every segment is axis-aligned
    for (let i = 0; i < pts.length - 1; i++) {
      const dx = Math.abs(pts[i + 1].x - pts[i].x);
      const dy = Math.abs(pts[i + 1].y - pts[i].y);
      expect(dx === 0 || dy === 0).toBe(true);
    }
  });

  it("glues to shapes and re-routes when a shape moves", () => {
    const el = makeElement("connector", { startId: boxA.id, endId: boxB.id, routing: "elbow" });
    const before = routeConnector(el, elements);
    expect(before[0].x).toBeCloseTo(100, 8); // exits A's right edge
    expect(before[before.length - 1].x).toBeCloseTo(300, 8); // enters B's left edge

    const moved = [...elements.map((e) => (e.id === boxB.id ? { ...e, data: { ...e.data, x: 500 } } : e))];
    const after = routeConnector(el, moved);
    expect(after[after.length - 1].x).toBeCloseTo(500, 8);
  });

  it("falls back to stored points when a glued id is missing", () => {
    const el = makeElement("connector", { startId: "ghost", endId: boxB.id, startPoint: { x: 0, y: 0 }, routing: "straight" });
    const pts = routeConnector(el, elements);
    expect(pts[0]).toEqual({ x: 0, y: 0 });
    expect(pts[pts.length - 1].x).toBeCloseTo(300, 8);
  });

  it("splices waypoints into the path", () => {
    const el = makeElement("connector", {
      startPoint: { x: 0, y: 0 },
      endPoint: { x: 200, y: 0 },
      waypoints: [{ x: 100, y: 80 }],
      routing: "elbow",
    });
    const pts = routeConnector(el, elements);
    // the route must pass through (100, 80)
    expect(pts.some((p) => p.x === 100 && p.y === 80)).toBe(true);
  });
});

describe("elementBounds — connector", () => {
  it("returns the AABB of the routed path", () => {
    const boxA = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
    const boxB = makeElement("rectangle", { x: 300, y: 0, width: 100, height: 50 });
    const el = makeElement("connector", { startId: boxA.id, endId: boxB.id, routing: "straight" });
    const b = elementBounds(el, [boxA, boxB]);
    expect(b.x).toBeCloseTo(100, 8);
    expect(b.y).toBeCloseTo(25, 8);
    expect(b.width).toBeGreaterThan(100);
    expect(b.height).toBeLessThanOrEqual(1e-6);
  });
  it("falls back to zero bounds without elements", () => {
    const el = makeElement("connector", { startId: "ghost", endId: "other", routing: "straight" });
    expect(elementBounds(el)).toEqual({ x: 0, y: 0, width: 0, height: 0 });
  });
});
