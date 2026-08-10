import { describe, it, expect } from "vitest";
import { makeElement } from "../stores/model";
import {
  translateElement,
  resizeElement,
  rotateElementTransform,
  currentRotation,
  applyTransform,
  invertTransformPoint,
  cloneElementOffset,
} from "./selection";

describe("translateElement", () => {
  it("shifts x/y for box types (rectangle/ellipse/sticky/text/image)", () => {
    const el = makeElement("rectangle", { x: 10, y: 20, width: 100, height: 50 });
    const moved = translateElement(el, 5, -3);
    expect(moved.data).toMatchObject({ x: 15, y: 17, width: 100, height: 50 });
  });

  it("shifts both endpoints for line/arrow", () => {
    const line = makeElement("line", { x1: 0, y1: 0, x2: 40, y2: 20 });
    const moved = translateElement(line, 10, 10);
    expect(moved.data).toEqual({ x1: 10, y1: 10, x2: 50, y2: 30 });

    const arrow = makeElement("arrow", { x1: 5, y1: 5, x2: 15, y2: 45 });
    const movedArrow = translateElement(arrow, -5, 2);
    expect(movedArrow.data).toEqual({ x1: 0, y1: 7, x2: 10, y2: 47 });
  });

  it("shifts every point for freehand, preserving pressure", () => {
    const el = makeElement("freehand", {
      points: [
        [0, 0, 0.4],
        [10, 5, 0.6],
      ],
    });
    const moved = translateElement(el, 2, 3);
    expect(moved.data.points).toEqual([
      [2, 3, 0.4],
      [12, 8, 0.6],
    ]);
  });

  it("does not mutate the original element", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 10, height: 10 });
    translateElement(el, 5, 5);
    expect(el.data).toMatchObject({ x: 0, y: 0 });
  });

  it("re-centers a rotated element's transform on the new position, keeping its angle", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 20, height: 20 });
    const rotated = rotateElementTransform(el, Math.PI / 2);
    const moved = translateElement(rotated, 100, 100);
    expect(currentRotation(moved)).toBeCloseTo(Math.PI / 2, 10);
    // Rotating 90° about the (moved) center and mapping the center itself must be a fixed point.
    const cx = moved.data.x + moved.data.width / 2;
    const cy = moved.data.y + moved.data.height / 2;
    const p = applyTransform(moved.transform, { x: cx, y: cy });
    expect(p.x).toBeCloseTo(cx, 8);
    expect(p.y).toBeCloseTo(cy, 8);
  });
});

describe("resizeElement", () => {
  it("grows a box from the se handle", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
    const resized = resizeElement(el, "se", 20, 10);
    expect(resized.data).toMatchObject({ x: 0, y: 0, width: 120, height: 60 });
  });

  it("moves x/width from the w handle, leaving the right edge fixed", () => {
    const el = makeElement("rectangle", { x: 10, y: 10, width: 100, height: 50 });
    const resized = resizeElement(el, "w", 15, 0);
    expect(resized.data).toMatchObject({ x: 25, y: 10, width: 85, height: 50 });
  });

  it("flips the box (never goes negative) when a handle is dragged past the opposite edge", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 50, height: 50 });
    // Drag the se handle (initially at 50,50) by (-70,-70) -> past the nw corner
    // (0,0) by 20px in each axis. The box should flip, not collapse/go negative.
    const resized = resizeElement(el, "se", -70, -70);
    expect(resized.data.width).toBeGreaterThanOrEqual(0);
    expect(resized.data.height).toBeGreaterThanOrEqual(0);
    expect(resized.data).toMatchObject({ x: -20, y: -20, width: 20, height: 20 });
  });

  it("moves the nearest endpoint for a line", () => {
    const el = makeElement("line", { x1: 0, y1: 0, x2: 100, y2: 0 });
    // The "nw" handle sits at the line's bounding-box top-left, i.e. right at (x1,y1).
    const resized = resizeElement(el, "nw", 10, 5);
    expect(resized.data).toEqual({ x1: 10, y1: 5, x2: 100, y2: 0 });
  });

  it("leaves freehand elements unchanged (unsupported resize target)", () => {
    const el = makeElement("freehand", { points: [[0, 0, 0.5], [10, 10, 0.5]] });
    const resized = resizeElement(el, "se", 50, 50);
    expect(resized).toBe(el);
  });
});

describe("rotateElementTransform", () => {
  it("produces the standard rotate-about-center canvas matrix", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 20, height: 20 }); // center (10,10)
    const angle = Math.PI / 2;
    const rotated = rotateElementTransform(el, angle);
    const cos = Math.cos(angle);
    const sin = Math.sin(angle);
    expect(rotated.transform.a).toBeCloseTo(cos, 10);
    expect(rotated.transform.b).toBeCloseTo(sin, 10);
    expect(rotated.transform.c).toBeCloseTo(-sin, 10);
    expect(rotated.transform.d).toBeCloseTo(cos, 10);
    expect(rotated.transform.e).toBeCloseTo(10 * (1 - cos) + 10 * sin, 10);
    expect(rotated.transform.f).toBeCloseTo(10 * (1 - cos) - 10 * sin, 10);
  });

  it("maps the element's own top-left corner to the expected world point at 90°", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 20, height: 20 }); // center (10,10)
    const rotated = rotateElementTransform(el, Math.PI / 2);
    // Rotating (0,0) by +90° about (10,10): the point (-10,-10) relative to center
    // becomes (10,-10) relative to center -> world (20, 0).
    const p = applyTransform(rotated.transform, { x: 0, y: 0 });
    expect(p.x).toBeCloseTo(20, 8);
    expect(p.y).toBeCloseTo(0, 8);
  });

  it("angle 0 yields the identity matrix", () => {
    const el = makeElement("rectangle", { x: 5, y: 5, width: 30, height: 10 });
    const { a, b, c, d, e, f } = rotateElementTransform(el, 0).transform;
    // Use +0 comparisons (not toEqual) since cos/sin(0) can produce a signed -0.
    expect([a, b, c, d, e, f].map((n) => n + 0)).toEqual([1, 0, 0, 1, 0, 0]);
  });

  it("currentRotation round-trips the angle it set", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 20, height: 20 });
    const rotated = rotateElementTransform(el, 0.7);
    expect(currentRotation(rotated)).toBeCloseTo(0.7, 10);
  });
});

describe("invertTransformPoint", () => {
  it("is the exact inverse of applyTransform for a rotation matrix", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 40, height: 40 });
    const rotated = rotateElementTransform(el, 0.9);
    const local = { x: 3, y: 27 };
    const world = applyTransform(rotated.transform, local);
    const back = invertTransformPoint(rotated.transform, world);
    expect(back.x).toBeCloseTo(local.x, 8);
    expect(back.y).toBeCloseTo(local.y, 8);
  });

  it("passes points through unchanged for the identity transform", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 10, height: 10 });
    const p = { x: 12, y: -4 };
    expect(invertTransformPoint(el.transform, p)).toEqual(p);
  });
});

describe("cloneElementOffset", () => {
  it("produces a new id and seed, offset by the given delta", () => {
    const el = makeElement("rectangle", { x: 10, y: 10, width: 50, height: 50 });
    const clone = cloneElementOffset(el, 16, 16);
    expect(clone.id).not.toBe(el.id);
    expect(clone.seed).toBeTypeOf("number");
    expect(clone.data).toMatchObject({ x: 26, y: 26, width: 50, height: 50 });
    expect(clone.elementType).toBe(el.elementType);
  });

  it("defaults to a 16px offset", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 10, height: 10 });
    const clone = cloneElementOffset(el);
    expect(clone.data).toMatchObject({ x: 16, y: 16 });
  });

  it("does not mutate the original element", () => {
    const el = makeElement("rectangle", { x: 0, y: 0, width: 10, height: 10 });
    const originalId = el.id;
    cloneElementOffset(el);
    expect(el.id).toBe(originalId);
    expect(el.data).toMatchObject({ x: 0, y: 0 });
  });
});
