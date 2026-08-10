import { describe, it, expect, beforeEach } from "bun:test";
import { DrawEngine } from "../src/draw-engine";

describe("DrawEngine board metadata", () => {
  let engine: DrawEngine;
  beforeEach(() => { engine = new DrawEngine(":memory:"); });

  it("createBoard fills metadata defaults", () => {
    const b = engine.createBoard("Hello");
    expect(b.name).toBe("Hello");
    expect(b.background).toBe("#1a1a2e");
    expect(b.gridSnap).toBe(false);
    expect(b.defaultStyleMode).toBe("clean");
    expect(b.elements).toEqual([]);
  });

  it("updateBoardMeta patches only given fields and bumps updatedAt", () => {
    const b = engine.createBoard("Shapes");
    engine.updateBoardMeta(b.id, { gridSnap: true, background: "#0f0f0f" });
    const after = engine.getBoard(b.id)!;
    expect(after.gridSnap).toBe(true);
    expect(after.background).toBe("#0f0f0f");
    expect(after.name).toBe("Shapes"); // untouched
  });

  it("deleteBoard removes the board", () => {
    const b = engine.createBoard("Temp");
    expect(engine.deleteBoard(b.id)).toBe(true);
    expect(engine.getBoard(b.id)).toBeUndefined();
    expect(engine.deleteBoard(b.id)).toBe(false);
  });

  it("elements survive create→updateElements→get round-trip", () => {
    const b = engine.createBoard("Doc");
    engine.updateElements(b.id, [{ id: "e1", order: 0 }]);
    expect(engine.getBoard(b.id)!.elements).toEqual([{ id: "e1", order: 0 }]);
  });
});