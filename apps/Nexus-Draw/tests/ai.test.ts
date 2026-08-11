import { describe, it, expect } from "bun:test";
import { aiElementsToServerElements, synthesizeDiagram } from "../src/ai";

describe("synthesizeDiagram", () => {
  it("is deterministic for the same prompt", () => {
    const a = synthesizeDiagram("login flow with auth and dashboard");
    const b = synthesizeDiagram("login flow with auth and dashboard");
    expect(JSON.stringify(a)).toBe(JSON.stringify(b));
  });

  it("produces at least two nodes and one arrow", () => {
    const els = synthesizeDiagram("plan");
    expect(els.length).toBeGreaterThanOrEqual(3);
    expect(els.some((e) => e.elementType === "arrow")).toBe(true);
  });

  it("each element has an id and integer order", () => {
    const els = synthesizeDiagram("design review");
    els.forEach((e, i) => {
      expect(typeof e.id).toBe("string");
      expect(e.order).toBe(i);
    });
  });

  it("arrows link consecutive node tops", () => {
    const els = synthesizeDiagram("flow");
    const boxes = els.filter((e) => e.elementType === "rectangle");
    const arrows = els.filter((e) => e.elementType === "arrow");
    expect(boxes.length).toBeGreaterThanOrEqual(2);
    expect(arrows.length).toBeGreaterThanOrEqual(1);
    for (let i = 0; i < Math.min(arrows.length, boxes.length - 1); i++) {
      const arrow = arrows[i]!;
      const from = boxes[i]!;
      const to = boxes[i + 1]!;
      expect(arrow.data.x1).toBe(from.data.x + from.data.width / 2);
      expect(arrow.data.y1).toBe(from.data.y + from.data.height);
      expect(arrow.data.x2).toBe(to.data.x + to.data.width / 2);
      expect(arrow.data.y2).toBe(to.data.y);
    }
  });
});

describe("aiElementsToServerElements", () => {
  it("passes synthesized elements through unchanged", () => {
    const els = synthesizeDiagram("plan");
    expect(aiElementsToServerElements(els)).toEqual(els);
  });
});
