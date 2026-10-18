import { describe, expect, it } from "bun:test";
import { aiElementsToServerElements, synthesizeDiagram } from "../src/ai";

type NumericData = {
  x: number;
  y: number;
  width: number;
  height: number;
  x1: number;
  y1: number;
  x2: number;
  y2: number;
};

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
      const arrow = arrows[i];
      const from = boxes[i];
      const to = boxes[i + 1];
      if (!arrow || !from || !to) throw new Error("synthesizeDiagram returned incomplete links");
      const arrowData = arrow.data as NumericData;
      const fromData = from.data as NumericData;
      const toData = to.data as NumericData;
      expect(arrowData.x1).toBe(fromData.x + fromData.width / 2);
      expect(arrowData.y1).toBe(fromData.y + fromData.height);
      expect(arrowData.x2).toBe(toData.x + toData.width / 2);
      expect(arrowData.y2).toBe(toData.y);
    }
  });
});

describe("aiElementsToServerElements", () => {
  it("passes synthesized elements through unchanged", () => {
    const els = synthesizeDiagram("plan");
    expect(aiElementsToServerElements(els)).toEqual(els);
  });
});
