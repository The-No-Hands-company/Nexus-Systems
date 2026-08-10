import { describe, it, expect } from "vitest";
import {
  snapToGrid,
  dragShapeData,
  defaultStickyData,
  freehandData,
  textData,
  draftToElement,
  isDragShapeTool,
  GRID_SIZE,
} from "./toolController";

describe("isDragShapeTool", () => {
  it("accepts rectangle/ellipse/line/arrow/sticky", () => {
    for (const t of ["rectangle", "ellipse", "line", "arrow", "sticky"]) {
      expect(isDragShapeTool(t)).toBe(true);
    }
  });
  it("rejects pen/text/eraser/select/hand", () => {
    for (const t of ["pen", "text", "eraser", "select", "hand"]) {
      expect(isDragShapeTool(t)).toBe(false);
    }
  });
});

describe("snapToGrid", () => {
  it("passes points through when disabled", () => {
    expect(snapToGrid({ x: 17, y: 33 }, GRID_SIZE, false)).toEqual({ x: 17, y: 33 });
  });
  it("rounds to the nearest grid line when enabled", () => {
    expect(snapToGrid({ x: 17, y: 33 }, GRID_SIZE, true)).toEqual({ x: 0, y: 40 });
    expect(snapToGrid({ x: 21, y: 61 }, GRID_SIZE, true)).toEqual({ x: 40, y: 80 });
  });
});

describe("dragShapeData", () => {
  it("builds a normalized box for rectangle, regardless of drag direction", () => {
    expect(dragShapeData("rectangle", 100, 80, 20, 10)).toEqual({ x: 20, y: 10, width: 80, height: 70 });
  });
  it("builds a normalized box for ellipse", () => {
    expect(dragShapeData("ellipse", 10, 10, 60, 40)).toEqual({ x: 10, y: 10, width: 50, height: 30 });
  });
  it("builds endpoints (not a box) for line", () => {
    expect(dragShapeData("line", 0, 0, 50, 25)).toEqual({ x1: 0, y1: 0, x2: 50, y2: 25 });
  });
  it("builds endpoints (not a box) for arrow", () => {
    expect(dragShapeData("arrow", 5, 5, 15, 45)).toEqual({ x1: 5, y1: 5, x2: 15, y2: 45 });
  });
  it("builds a box plus empty text for sticky", () => {
    expect(dragShapeData("sticky", 0, 0, 100, 60)).toEqual({ x: 0, y: 0, width: 100, height: 60, text: "" });
  });
});

describe("defaultStickyData", () => {
  it("places a default-size sticky at the click point", () => {
    const d = defaultStickyData(12, 34);
    expect(d).toMatchObject({ x: 12, y: 34, text: "" });
    expect(d.width).toBeGreaterThan(0);
    expect(d.height).toBeGreaterThan(0);
  });
});

describe("freehandData", () => {
  it("wraps the accumulated points as-is", () => {
    const points = [
      [0, 0, 0.5],
      [1, 1, 0.6],
    ];
    expect(freehandData(points)).toEqual({ points });
  });
});

describe("textData", () => {
  it("carries position and text through", () => {
    expect(textData(10, 20, "hello")).toEqual({ x: 10, y: 20, text: "hello" });
  });
});

describe("draftToElement", () => {
  it("renders a shape draft as its element type with box/endpoint data", () => {
    const draft = { kind: "shape" as const, tool: "rectangle" as const, startX: 0, startY: 0, endX: 40, endY: 20, seed: 1 };
    const el = draftToElement(draft, "clean");
    expect(el.elementType).toBe("rectangle");
    expect(el.data).toEqual({ x: 0, y: 0, width: 40, height: 20 });
    expect(el.style.styleMode).toBe("clean");
  });
  it("renders a freehand draft as a freehand element", () => {
    const draft = { kind: "freehand" as const, points: [[0, 0, 0.5], [5, 5, 0.5]], seed: 2 };
    const el = draftToElement(draft, "sketch");
    expect(el.elementType).toBe("freehand");
    expect(el.data.points).toHaveLength(2);
  });
});
