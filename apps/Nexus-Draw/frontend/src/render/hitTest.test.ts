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
