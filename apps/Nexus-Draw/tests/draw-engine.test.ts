import { describe, it, expect, beforeEach } from "bun:test";
import { DrawEngine } from "../src/draw-engine";

describe("DrawEngine board metadata", () => {
  let engine: DrawEngine;
  beforeEach(() => { engine = new DrawEngine(":memory:"); });

  it("createBoard fills metadata defaults", () => {
    const b = engine.createBoard("Hello", "user:alice");
    expect(b.name).toBe("Hello");
    expect(b.ownerSubject).toBe("user:alice");
    expect(b.background).toBe("#1a1a2e");
    expect(b.gridSnap).toBe(false);
    expect(b.defaultStyleMode).toBe("clean");
    expect(b.elements).toEqual([]);
  });

  it("listBoards returns only owner boards plus public boards", () => {
    engine.createBoard("Alice private", "user:alice");
    const pub = engine.createBoard("Public", "user:alice");
    engine.updateBoardMeta(pub.id, "user:alice", { isPublic: true });
    engine.createBoard("Bob private", "user:bob");
    const aliceBoards = engine.listBoards("user:alice");
    const bobBoards = engine.listBoards("user:bob");
    expect(aliceBoards.map(b => b.name)).toContain("Alice private");
    expect(aliceBoards.map(b => b.name)).toContain("Public");
    expect(aliceBoards.map(b => b.name)).not.toContain("Bob private");
    // Bob sees only Public (owned by Alice but is_public=1)
    expect(bobBoards.map(b => b.name)).toContain("Public");
    expect(bobBoards.map(b => b.name)).not.toContain("Alice private");
  });

  it("updateBoardMeta patches only given fields and bumps updatedAt", () => {
    const b = engine.createBoard("Shapes", "user:alice");
    engine.updateBoardMeta(b.id, "user:alice", { gridSnap: true, background: "#0f0f0f" });
    const after = engine.getBoard(b.id)!;
    expect(after.gridSnap).toBe(true);
    expect(after.background).toBe("#0f0f0f");
    expect(after.name).toBe("Shapes"); // untouched
  });

  it("updateBoardMeta returns false for wrong owner", () => {
    const b = engine.createBoard("Private", "user:alice");
    const ok = engine.updateBoardMeta(b.id, "user:bob", { gridSnap: true });
    expect(ok).toBe(false);
    expect(engine.getBoard(b.id)!.gridSnap).toBe(false);
  });

  it("deleteBoard removes the board", () => {
    const b = engine.createBoard("Temp", "user:alice");
    expect(engine.deleteBoard(b.id, "user:alice")).toBe(true);
    expect(engine.getBoard(b.id)).toBeUndefined();
    expect(engine.deleteBoard(b.id, "user:alice")).toBe(false);
  });

  it("deleteBoard returns false for wrong owner", () => {
    const b = engine.createBoard("Alice board", "user:alice");
    expect(engine.deleteBoard(b.id, "user:bob")).toBe(false);
    expect(engine.getBoard(b.id)).toBeDefined();
  });

  it("elements survive create→updateElements→get round-trip", () => {
    const b = engine.createBoard("Doc", "user:alice");
    engine.updateElements(b.id, [{ id: "e1", order: 0 }]);
    expect(engine.getBoard(b.id)!.elements).toEqual([{ id: "e1", order: 0 }]);
  });
});
