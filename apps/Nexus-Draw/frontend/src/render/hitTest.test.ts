import { describe, it, expect } from "vitest";
import { elementBounds, hitElement, hitInMarquee } from "./hitTest";
import { makeElement } from "../stores/model";

const rect = makeElement("rectangle", { x:10, y:10, width:100, height:50 });

describe("elementBounds", () => {
  it("returns the rect's box", () => {
    expect(elementBounds(rect)).toEqual({ x:10, y:10, width:100, height:50 });
  });
});
describe("hitElement", () => {
  it("hits inside the rect", () => expect(hitElement(rect, {x:50,y:30}, 4)).toBe(true));
  it("misses outside", () => expect(hitElement(rect, {x:500,y:500}, 4)).toBe(false));

  // Task 7: locked and hidden elements are not pickable — a click must fall
  // through them to whatever is underneath, otherwise locking is cosmetic.
  it("never hits a locked element, even dead centre", () => {
    const locked = makeElement("rectangle", { x:10, y:10, width:100, height:50, locked: true });
    expect(hitElement(locked, {x:50,y:30}, 4)).toBe(false);
  });
  it("never hits a hidden element, even dead centre", () => {
    const hidden = makeElement("rectangle", { x:10, y:10, width:100, height:50, hidden: true });
    expect(hitElement(hidden, {x:50,y:30}, 4)).toBe(false);
  });
});
describe("hitInMarquee", () => {
  it("is selected when fully inside the marquee", () => {
    expect(hitInMarquee(rect, {x:0,y:0,width:200,height:200})).toBe(true);
  });
  it("is not selected when the marquee misses it", () => {
    expect(hitInMarquee(rect, {x:300,y:300,width:50,height:50})).toBe(false);
  });
});

describe("hitElement — connector", () => {
  const a = makeElement("rectangle", { x: 0, y: 0, width: 100, height: 50 });
  const b = makeElement("rectangle", { x: 300, y: 0, width: 100, height: 50 });
  const conn = makeElement("connector", { startId: a.id, endId: b.id, routing: "straight" });
  const elements = [a, b];

  it("hits a point on the line between the two shapes", () => {
    expect(hitElement(conn, { x: 200, y: 25 }, 4, elements)).toBe(true);
  });
  it("misses a point far from the line", () => {
    expect(hitElement(conn, { x: 200, y: 100 }, 4, elements)).toBe(false);
  });
  it("tolerates points within stroke+tolerance of the line", () => {
    expect(hitElement(conn, { x: 200, y: 30 }, 4, elements)).toBe(true);
  });
  it("never hits when elements are missing (falls back to origin points)", () => {
    expect(hitElement(conn, { x: 200, y: 25 }, 4)).toBe(false);
  });
});
